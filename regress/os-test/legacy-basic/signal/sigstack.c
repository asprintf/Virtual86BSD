/*[SUSv2]*/
/* Test whether a handler runs on the stack set by sigstack. */

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "../basic.h"

static char* stack;
static volatile sig_atomic_t on, reported;

/* the growth direction is not given: ss_sp is the middle of 2 * SIGSTKSZ */
static void handler(int sig)
{
	char here;
	struct sigstack ss;
	(void) sig;
	on = &here >= stack && &here < stack + 2 * SIGSTKSZ;
	if ( sigstack(NULL, &ss) == 0 )
		reported = ss.ss_onstack != 0;
}

int main(void)
{
	if ( !(stack = malloc(2 * SIGSTKSZ)) )
		err(1, "malloc");
	struct sigstack ss = { .ss_sp = stack + SIGSTKSZ, .ss_onstack = 1 }, old;
	if ( sigstack(&ss, &old) != 0 )
		err(1, "sigstack");
	if ( old.ss_onstack )
		errx(1, "sigstack said the process was on the signal stack");
	struct sigaction sa = { 0 };
	sa.sa_handler = handler;
	sa.sa_flags = SA_ONSTACK;
	sigaction(SIGUSR1, &sa, NULL);
	raise(SIGUSR1);
	if ( !on )
		errx(1, "the handler did not run on the signal stack");
	if ( !reported )
		errx(1, "sigstack in the handler did not say it was on the stack");
	return 0;
}
