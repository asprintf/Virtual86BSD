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

#ifndef _SIGNAL_H
#define _SIGNAL_H
#include <bits/features.h>

typedef int sig_atomic_t;

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int))-1)
#if __XSI_OB
#define SIG_HOLD ((void (*)(int))2)
#endif

#define SIGINT    2
#define SIGILL    4
#define SIGABRT   6
#define SIGFPE    8
#define SIGSEGV   11
#define SIGTERM   15

#if __POSIX_VISIBLE
#define __NEED_size_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_sigset_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

#define SIGHUP    1
#define SIGQUIT   3
#define SIGTRAP   5
#define SIGIOT    SIGABRT
#define SIGBUS    10
#define SIGKILL   9
#define SIGSYS    12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGURG    16
#define SIGSTOP   17
#define SIGTSTP   18
#define SIGCONT   19
#define SIGCHLD   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGIO     23
#define SIGPOLL   SIGIO
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGUSR1   30
#define SIGUSR2   31

/* 32-34 are reserved for the library (thread cancellation, set*id
 * broadcast, timers); 65-126 are the realtime signals. */
#define SIGRTMIN  65
#define SIGRTMAX  126
#define _NSIG     127

#define SIG2STR_MAX 32

#include <bits/siginfo.h>

struct sigevent {
	int sigev_notify;
	int sigev_signo;
	union sigval sigev_value;
	union {
		int __tid;
		struct {
			void (*__function)(union sigval);
			pthread_attr_t *__attr;
		} __thr;
		long __pad[8];
	} __un;
};
#define sigev_notify_function   __un.__thr.__function
#define sigev_notify_attributes __un.__thr.__attr

#define SIGEV_NONE   0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

struct sigaction {
	union {
		void (*__sa_handler)(int);
		void (*__sa_sigaction)(int, siginfo_t *, void *);
	} __sa_handler;
	int sa_flags;
	sigset_t sa_mask;
};
#define sa_handler   __sa_handler.__sa_handler
#define sa_sigaction __sa_handler.__sa_sigaction

#define SA_ONSTACK   0x0001
#define SA_RESTART   0x0002
#define SA_RESETHAND 0x0004
#define SA_NOCLDSTOP 0x0008
#define SA_NODEFER   0x0010
#define SA_NOCLDWAIT 0x0020
#define SA_SIGINFO   0x0040

#define SIG_BLOCK   1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

typedef struct {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} stack_t;

#if __XSI_OB5
/* SUSv2 (LEGACY): sigstack()'s, members in glibc's order */
struct sigstack {
	void *ss_sp;
	int ss_onstack;
};
#endif

#if __XSI_VISIBLE
#define SS_ONSTACK  1
#define SS_DISABLE  4
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    (MINSIGSTKSZ + 32768)
#endif

/* Machine context: the kernel's (sys/amd64/include/ucontext.h). */
typedef struct __mcontext {
	long __mc_onstack;
	long __mc_rdi, __mc_rsi, __mc_rdx, __mc_rcx, __mc_r8, __mc_r9, __mc_rax, __mc_rbx, __mc_rbp;
	long __mc_r10, __mc_r11, __mc_r12, __mc_r13, __mc_r14, __mc_r15;
	unsigned int __mc_trapno;
	unsigned short __mc_fs, __mc_gs;
	long __mc_addr;
	unsigned int __mc_flags;
	unsigned short __mc_es, __mc_ds;
	long __mc_err, __mc_rip, __mc_cs, __mc_rflags, __mc_rsp, __mc_ss;
	long __mc_len;
	long __mc_fpformat;
	long __mc_ownedfp;
	long __mc_fpstate[64] __attribute__((aligned(64)));
	long __mc_fsbase, __mc_gsbase;
	long __mc_xfpustate, __mc_xfpustate_len;
	long __mc_spare[4];
} mcontext_t;

typedef struct __ucontext {
	sigset_t uc_sigmask;
	mcontext_t uc_mcontext;
	struct __ucontext *uc_link;
	stack_t uc_stack;
	int uc_flags;
	int __spare__[4];
} ucontext_t;

/* si_code values */
#define SI_USER    0x10001
#define SI_QUEUE   0x10002
#define SI_TIMER   0x10003
#define SI_ASYNCIO 0x10004
#define SI_MESGQ   0x10005
#define SI_KERNEL  0x10006
#define SI_LWP     0x10007

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8

#define FPE_INTOVF 1
#define FPE_INTDIV 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8

#define SEGV_MAPERR 1
#define SEGV_ACCERR 2

#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3

#if __XSI_VISIBLE
#define TRAP_BRKPT 1
#define TRAP_TRACE 2
#endif

#define CLD_EXITED    1
#define CLD_KILLED    2
#define CLD_DUMPED    3
#define CLD_TRAPPED   4
#define CLD_STOPPED   5
#define CLD_CONTINUED 6

/* SIGPOLL's codes: in SUSv2 to SUSv4 (STREAMS), not in POSIX.1-2024 */
#if __XSI_OB
#define POLL_IN  1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6
#endif
#endif /* __POSIX_VISIBLE */

__BEGIN_DECLS
int raise(int);
void (*signal(int, void (*)(int)))(int);
#if __POSIX_VISIBLE
int  kill(pid_t, int);
void psiginfo(const siginfo_t *, const char *);
void psignal(int, const char *);
int  pthread_kill(pthread_t, int);
int  pthread_sigmask(int, const sigset_t *__RESTRICT, sigset_t *__RESTRICT);
int  sig2str(int, char *);
int  sigaction(int, const struct sigaction *__RESTRICT, struct sigaction *__RESTRICT);
int  sigaddset(sigset_t *, int);
int  sigdelset(sigset_t *, int);
int  sigemptyset(sigset_t *);
int  sigfillset(sigset_t *);
int  sigismember(const sigset_t *, int);
int  sigpending(sigset_t *);
int  sigprocmask(int, const sigset_t *__RESTRICT, sigset_t *__RESTRICT);
int  sigqueue(pid_t, int, union sigval);
int  sigsuspend(const sigset_t *);
int  sigtimedwait(const sigset_t *__RESTRICT, siginfo_t *__RESTRICT, const struct timespec *__RESTRICT);
int  sigwait(const sigset_t *__RESTRICT, int *__RESTRICT);
int  sigwaitinfo(const sigset_t *__RESTRICT, siginfo_t *__RESTRICT);
int  str2sig(const char *__RESTRICT, int *__RESTRICT);
#if __XSI_VISIBLE
int  killpg(pid_t, int);
int  sigaltstack(const stack_t *__RESTRICT, stack_t *__RESTRICT);
#endif
#if __XSI_OB
/* SUSv2-SUSv4, obsolescent, removed in POSIX.1-2024 */
int  sighold(int);
int  sigignore(int);
int  siginterrupt(int, int);
int  sigpause(int);
int  sigrelse(int);
void (*sigset(int, void (*)(int)))(int);
#endif
#if __XSI_OB6
/* SUSv2, SUSv3 (obsolescent there); removed in POSIX.1-2008 */
void (*bsd_signal(int, void (*)(int)))(int);
#endif
#if __XSI_OB5
/* SUSv2 (LEGACY) */
int  sigstack(struct sigstack *, struct sigstack *);
#endif
#endif
__END_DECLS
#endif
