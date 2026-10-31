/* Internal: the thread control block, and the internals of <pthread.h>.
 *
 * %fs points at this structure (x86-64 TLS variant II: the static TLS
 * blocks lie immediately below it).  The first fields follow the layout
 * compilers rely on: self pointer at %fs:0 and the stack-protector canary
 * at %fs:0x28.
 *
 * Thread life cycle (detach_state): JOINABLE or DETACHED while running;
 * an exiting joinable thread moves to EXITING, and the kernel writes
 * EXITED (0) there when the thread is gone (CLONE_CHILD_CLEARTID) and
 * wakes pthread_join().  A detached thread unmaps its own stack.
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
	unsigned long sigmask;
	volatile int startlock;       /* 1: wait for the creator; 2: go; 3: give up */
	volatile int timer_id;        /* SIGEV_THREAD timers: the kernel timer */
	struct {                      /* the kernel's robust list (set_robust_list) */
		volatile void *volatile head;
		long off;
		volatile void *volatile pending;
	} robust_list;
	const char *dlerror;          /* <dlfcn.h>: the pending error message */
	unsigned long *dtv_static;    /* the dtv in the TLS area (dlopen: dtv may be another) */
	char *dlerror_buf;            /* libc.so: the last formatted message (malloc) */
	/* PTHREAD_PRIO_PROTECT: how many mutexes of each ceiling (1-99) the
	 * thread holds, the highest, and its own priority while raised */
	int pp_max, pp_base;
	unsigned short pp_count[100];
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

/* mutexes (int __i[10], void *__p[5]) */
#define _m_type  __i[0]   /* type 0-1 (normal, recursive, errorcheck), 4 robust,
                           * 8 inconsistent, 16/32/64 protocol (below),
                           * 128 process-shared, ceiling << 16 */
#define _m_lock  __i[1]   /* 0, or owner tid | FUTEX_WAITERS; NOTRECOVERABLE */
#define _m_count __i[2]   /* recursion */
#define _m_prev  __p[2]   /* robust list */
#define _m_next  __p[3]
#define __M_ROBUST 4
#define __M_INCONSISTENT 8
#define __M_PI 16         /* PTHREAD_PRIO_INHERIT: the kernel's PI futexes */
#define __M_PP 32         /* PTHREAD_PRIO_PROTECT; the ceiling in bits 16-23 */
#define __M_PI_NOTREC 64  /* a robust PI mutex made unrecoverable (the kernel
                           * owns its lock word while contended) */
#define __M_SHARED 128
#define __M_CEIL(t) (((unsigned)(t) >> 16) & 255)
#define __M_NOTRECOVERABLE 0x3fffffff
#define FUTEX_WAITERS 0x80000000u
#define FUTEX_OWNER_DIED 0x40000000

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

int __clone(int (*)(void *), void *stack, int flags, void *arg, int *ptid, void *tls, volatile int *ctid);
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
void __block_all_sigs(unsigned long *old);
void __restore_sigs(const unsigned long *old);
int __libc_sigaction(int sig, const void *sa, void *old);   /* no reserved-signal check */
int __robust_register(struct __pthread *);

#endif
