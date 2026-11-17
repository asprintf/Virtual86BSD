/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Internal: the thread control block, and the internals of <pthread.h>.
 *
 * %fs points at this structure (x86-64 TLS variant II: the static TLS
 * blocks lie immediately below it).  The first fields follow the layout
 * compilers rely on: self pointer at %fs:0 and the stack-protector canary
 * at %fs:0x28.
 *
 * Thread life cycle (detach_state): JOINABLE or DETACHED while running;
 * an exiting joinable thread moves to EXITING, and the kernel writes 1 to
 * `dead' when the thread is gone (thr_exit) and wakes pthread_join().  A
 * detached thread unmaps its own stack.
 *
 * Cancellation: system calls that are cancellation points go through
 * __syscall_cp_asm (arch/x86_64/syscall_cp.s), which tests `cancel'
 * before entering the kernel.  SIGCANCEL's handler moves a thread that
 * is between that test and the end of the syscall instruction to
 * __cp_cancel, so a cancellation request can neither be lost nor act
 * after the system call has had its effect.
 */
#ifndef _INTERNAL_PTHREAD_IMPL_H
#define _INTERNAL_PTHREAD_IMPL_H
#include <stddef.h>
#include <time.h>
#include "syscall.h"

struct __ptcb;

struct __pthread {
	struct __pthread *self;       /* 0x00 */
	unsigned long *dtv;           /* 0x08 dynamic thread vector */
	struct __pthread *self2;      /* 0x10 */
	int __pad0[2];                /* 0x18 */
	unsigned long sysinfo;        /* 0x20 */
	unsigned long canary;         /* 0x28 */
	unsigned long pointer_guard;  /* 0x30 */
	/* Library-private part. */
	struct __pthread *next, *prev;
	int tid;
	int __tid_hi;                 /* (the kernel stores the tid as a long) */
	int errno_val;
	volatile int detach_state;
	volatile int cancel;
	volatile unsigned char canceldisable, cancelasync;
	unsigned char tsd_used, robust_registered;
	unsigned char c11;            /* thrd_create: the start routine returns int */
	void *map_base;
	size_t map_size;
	void *stack;
	size_t stack_size;
	size_t guard_size;
	void *result;
	struct __ptcb *cancelbuf;
	void **tsd;
	void *locale;
	volatile int killlock;
	void *(*start)(void *);
	void *start_arg;
	__sm_t sigmask;
	volatile int startlock;       /* 1: wait for the creator; 2: go; 3: give up */
	volatile int timer_id;        /* SIGEV_THREAD timers: the kernel timer */
	volatile long dead;           /* the kernel sets it to 1 at thr_exit */
	struct {                      /* the robust lists the kernel walks when the thread ends */
		volatile void *volatile head;      /* the first robust mutex held */
		volatile void *volatile priv;      /* (the private list: unused) */
		volatile void *volatile pending;   /* the one being locked */
	} robust_list;
	const char *dlerror;          /* <dlfcn.h>: the pending error message */
	unsigned long *dtv_static;    /* the dtv in the TLS area (dlopen: dtv may be another) */
	char *dlerror_buf;            /* libc.so: the last formatted message (malloc) */
	void *pp_top;                 /* the PTHREAD_PRIO_PROTECT mutex locked last (_m_ppnext the rest) */
};

enum { DT_EXITED = 0, DT_EXITING, DT_JOINABLE, DT_DETACHED };

/* canceldisable: PTHREAD_CANCEL_ENABLE 0, _DISABLE 1, and internally
 * MASKED: a cancellation point returns ECANCELED instead of acting */
#define __CANCEL_MASKED 2

/* signals of the library (see <signal.h>) */
#define SIGCANCEL 32
#define SIGSETXID 33
#define SIGTIMER  34

#if defined(__GNUC__) || defined(__clang__)
static inline struct __pthread *__pthread_self(void)
{
	struct __pthread *self;
	__asm__ ("mov %%fs:0,%0" : "=r"(self));
	return self;
}
#else
struct __pthread *__get_tp(void);
#define __pthread_self() __get_tp()
#endif

/* pthread_t attributes (pthread_attr_t is long __l[7]) */
#define _a_stacksize __l[0]
#define _a_guardsize __l[1]
#define _a_stackaddr __l[2]
#define _a_detach    __l[3]
#define _a_sched     __l[4]   /* bit 0 explicit; policy << 8 */
#define _a_prio      __l[5]
#define _a_flags     __l[6]   /* bit 0: set; bit 1: C11 thread */

/* mutexes (int __i[10], void *__p[5]): the kernel's struct umutex in the first
 * 32 bytes (the lock word, its flags, the ceiling, the link of the robust
 * list), then the library's type and count */
#define _m_lock   __i[0]  /* m_owner: 0, or owner tid, | UMUTEX_CONTESTED while others sleep;
                           * UMUTEX_RB_OWNERDEAD, UMUTEX_RB_NOTRECOV (robust) */
#define _m_kflags __i[1]  /* m_flags: the UMUTEX_ flags the kernel reads */
#define _m_kceil  __i[2]  /* m_ceilings[0]: the POSIX priority ceiling (PP) */
#define _m_kceil1 __i[3]  /* m_ceilings[1]: the ceiling the owner keeps at a PP unlock, or -1 */
#define _m_rb     __p[2]  /* m_rb_lnk: the next robust mutex of the owner */
#define _m_ppnext __p[3]  /* (m_spare) the PP mutex the owner locked before this one */
#define _m_type   __i[8]  /* type 0-1 (normal, recursive, errorcheck), 4 robust,
                           * 16/32 protocol (below), 128 process-shared, ceiling << 16 */
#define _m_count  __i[9]  /* recursion */
#define __M_ROBUST 4
#define __M_PI 16         /* PTHREAD_PRIO_INHERIT: the kernel's PI umutex */
#define __M_PP 32         /* PTHREAD_PRIO_PROTECT; the ceiling in bits 16-23 */
#define __M_SHARED 128
#define __M_CEIL(t) (((unsigned)(t) >> 16) & 255)
/* the kernel's m_flags and lock word values (sys/sys/umtx.h) */
#define UMUTEX_SHARED       0x0001
#define UMUTEX_PRIO_INHERIT 0x0004
#define UMUTEX_PRIO_PROTECT 0x0008
#define UMUTEX_ROBUST       0x0010
#define UMUTEX_NONCONSISTENT 0x0020
#define UMUTEX_CONTESTED    0x80000000u

/* condition variables (int __i[12], void *__p[6]) */
#define _c_head   __p[0]
#define _c_tail   __p[1]
#define _c_lock   __i[4]
#define _c_flags  __i[5]   /* clock id; 128 process-shared */
#define _c_seq    __i[6]   /* process-shared: wait sequence */
#define _c_waiters __i[7]

/* rwlocks (int __i[14]) */
#define _rw_lock    __i[0]  /* readers, or 0x7fffffff; 0x80000000 waiters */
#define _rw_waiters __i[1]
#define _rw_shared  __i[2]  /* 128 process-shared */

/* barriers (int __i[8]) */
#define _b_lock   __i[0]
#define _b_count  __i[1]
#define _b_limit  __i[2]
#define _b_seq    __i[3]
#define _b_shared __i[4]

/* the list of threads (circular, through next/prev, from the main thread) */
extern volatile int __thread_list_lock;
void __tl_lock(void);
void __tl_unlock(void);

/* futex waits: 0, or ETIMEDOUT, EINTR, EINVAL; the _cp form is a
 * cancellation point (and returns ECANCELED when masked) */
int __timedwait(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv);
int __timedwait_cp(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv);
void __wake(volatile int *addr, int cnt, int priv);
void __wait(volatile int *addr, int val, int priv);

long __syscall_cp_c(long nr, long a, long b, long c, long d, long e, long f);
long __cancel(void);
void __testcancel(void);
int __setcancelstate(int new, int *old);

_Noreturn void __unmapself(void *base, size_t size);

/* the implementations behind the public names (which are weak aliases,
 * so that the C11 <threads.h> functions bring no non-ISO names along) */
typedef struct __pthread *__pthread_t;
union __pthread_attr_u;
int __pthread_create(__pthread_t *, const void *attr, void *(*)(void *), void *);
_Noreturn void __pthread_exit(void *);
int __pthread_join(__pthread_t, void **);
int __pthread_detach(__pthread_t);
int __pthread_kill(__pthread_t, int sig);   /* internal signals allowed */
void __pthread_tsd_run_dtors(void);
void __block_all_sigs(__sm_t *old);
void __restore_sigs(const __sm_t *old);
int __libc_sigaction(int sig, const void *sa, void *old);   /* no reserved-signal check */
int __robust_register(struct __pthread *);
void __robust_exit(struct __pthread *);
long __thr_setsched(long tid, int policy, int prio);          /* 0, or -errno */
long __thr_getsched(long tid, int *policy, int *prio);

#endif
