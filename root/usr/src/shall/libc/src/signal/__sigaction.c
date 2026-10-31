#include <signal.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

void __restore_rt(void);

#define SA_RESTORER 0x04000000

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned long mask;
};

/* also for the library's own signals 32-34 (pthread_impl.h) */
int __libc_sigaction(int sig, const void *sav, void *oldv)
{
	const struct sigaction *sa = sav;
	struct sigaction *old = oldv;
	struct k_sigaction ksa, kold;
	long r;
	if ((unsigned)sig - 1 >= _NSIG - 1) { errno = EINVAL; return -1; }
	if (sa) {
		ksa.handler = sa->sa_handler;
		ksa.flags = (unsigned long)(unsigned)sa->sa_flags | SA_RESTORER;
		ksa.restorer = __restore_rt;
		ksa.mask = sa->sa_mask.__bits[0];
	}
	r = __syscall4(SYS_rt_sigaction, sig, sa ? &ksa : 0, old ? &kold : 0, 8);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	if (old) {
		memset(old, 0, sizeof *old);
		old->sa_handler = kold.handler;
		old->sa_flags = (int)(kold.flags & ~(unsigned long)SA_RESTORER);
		old->sa_mask.__bits[0] = kold.mask;
	}
	return 0;
}

int __sigaction(int sig, const struct sigaction *__RESTRICT sa, struct sigaction *__RESTRICT old)
{
	if ((unsigned)sig - 32 < 3) { errno = EINVAL; return -1; }
	return __libc_sigaction(sig, sa, old);
}
