#include <stdlib.h>
#include "syscall.h"
#include "atomic.h"

#define SIGABRT 6

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned long mask;
};

_Noreturn void abort(void)
{
	unsigned long all = -1UL, abrt = 1UL << (SIGABRT - 1);
	struct k_sigaction sa = { 0, 0, 0, 0 }; /* SIG_DFL */
	int tid;

	/* First give a SIGABRT handler its chance. */
	tid = (int)__syscall0(SYS_gettid);
	__syscall2(SYS_tkill, tid, SIGABRT);

	/* It returned (or the signal was blocked/ignored): force default. */
	__syscall4(SYS_rt_sigprocmask, 0 /*SIG_BLOCK*/, &all, 0, 8);
	__syscall4(SYS_rt_sigaction, SIGABRT, &sa, 0, 8);
	__syscall2(SYS_tkill, tid, SIGABRT);
	__syscall4(SYS_rt_sigprocmask, 1 /*SIG_UNBLOCK*/, &abrt, 0, 8);

	/* Should be unreachable. */
	a_crash();
	for (;;) __syscall1(SYS_exit_group, 127);
}
