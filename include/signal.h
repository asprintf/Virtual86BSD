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
#define SIGBUS    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGPOLL   29
#define SIGIO     SIGPOLL
#define SIGPWR    30
#define SIGSYS    31

/* 32-34 are reserved for the library (thread cancellation, set*id
 * broadcast, timers); 35-64 are the realtime signals. */
#define SIGRTMIN  35
#define SIGRTMAX  64
#define _NSIG     65

#define SIG2STR_MAX 32

#include <bits/siginfo.h>

struct sigevent {
	union sigval sigev_value;
	int sigev_signo;
	int sigev_notify;
	union {
		int __pad[12];
		int __tid;
		struct {
			void (*__function)(union sigval);
			pthread_attr_t *__attr;
		} __thr;
	} __un;
};
#define sigev_notify_function   __un.__thr.__function
#define sigev_notify_attributes __un.__thr.__attr

#define SIGEV_SIGNAL 0
#define SIGEV_NONE   1
#define SIGEV_THREAD 2

struct sigaction {
	union {
		void (*__sa_handler)(int);
		void (*__sa_sigaction)(int, siginfo_t *, void *);
	} __sa_handler;
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
};
#define sa_handler   __sa_handler.__sa_handler
#define sa_sigaction __sa_handler.__sa_sigaction

#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO   4
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

#if __XSI_VISIBLE
#define SS_ONSTACK  1
#define SS_DISABLE  2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192
#endif

/* Machine context: the kernel's x86-64 sigcontext. */
typedef struct {
	long long __gregs[23];
	void *__fpregs;
	unsigned long long __reserved[8];
} mcontext_t;

typedef struct __ucontext {
	unsigned long uc_flags;
	struct __ucontext *uc_link;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
	sigset_t uc_sigmask;
	unsigned long __fpregs_mem[64];
} ucontext_t;

/* si_code values */
#define SI_USER    0
#define SI_KERNEL  0x80
#define SI_QUEUE   (-1)
#define SI_TIMER   (-2)
#define SI_MESGQ   (-3)
#define SI_ASYNCIO (-4)
#define SI_SIGIO   (-5)
#define SI_TKILL   (-6)

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8

#define FPE_INTDIV 1
#define FPE_INTOVF 2
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
#endif
__END_DECLS
#endif
