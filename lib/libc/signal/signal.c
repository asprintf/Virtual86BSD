/* signal() and raise() are ISO C: they may use only reserved names. */
#include <signal.h>
#include <string.h>
#include "syscall.h"

int __sigaction(int, const struct sigaction *, struct sigaction *);

void (*signal(int sig, void (*func)(int)))(int)
{
	struct sigaction sa, old;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = func;
	sa.sa_flags = SA_RESTART;
	if (__sigaction(sig, &sa, &old) < 0) return SIG_ERR;
	return old.sa_handler;
}
