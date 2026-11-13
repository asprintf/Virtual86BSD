/*[SUSv3]*/
/* Test whether setcontext of a signal handler's context resumes the interrupted code. */

#include <signal.h>
#include <ucontext.h>

#include "../basic.h"

static volatile sig_atomic_t handled;

static void handler(int sig, siginfo_t* info, void* context)
{
	(void) sig;
	(void) info;
	handled++;
	/* the same as returning: the interrupted code goes on */
	setcontext((ucontext_t*) context);
}

int main(void)
{
	struct sigaction sa = { 0 };
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, NULL);
	volatile long a = 1, b = 2, c = 3;
	raise(SIGUSR1);
	if ( handled != 1 || a + b + c != 6 )
		errx(1, "the interrupted code did not go on as it was");
	/* the signal mask came back without SIGUSR1 */
	raise(SIGUSR1);
	if ( handled != 2 )
		errx(1, "SIGUSR1 stayed blocked after setcontext");
	return 0;
}
