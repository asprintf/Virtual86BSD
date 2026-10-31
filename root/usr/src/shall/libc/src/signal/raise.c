#include <signal.h>
#include "syscall.h"

int raise(int sig)
{
	unsigned long all = -1UL, old;
	long r;
	/* block everything so the signal is delivered on return from the
	 * mask restore, i.e. before raise() returns */
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &all, &old, 8);
	r = __syscall2(SYS_tkill, __syscall0(SYS_gettid), sig);
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &old, 0, 8);
	return (int)__syscall_ret((unsigned long)r);
}
