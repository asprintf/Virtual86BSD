/*[SUSv3]*/
/* Test whether bsd_signal installs a restarting handler that blocks its signal. */

#include <signal.h>

#include "../basic.h"

static void handler(int sig)
{
	(void) sig;
}

int main(void)
{
	if ( bsd_signal(SIGUSR1, handler) != SIG_DFL )
		errx(1, "bsd_signal did not return the previous action");
	struct sigaction sa;
	sigaction(SIGUSR1, NULL, &sa);
	if ( sa.sa_handler != handler )
		errx(1, "bsd_signal did not install the handler");
	if ( !(sa.sa_flags & SA_RESTART) || (sa.sa_flags & SA_RESETHAND) )
		errx(1, "bsd_signal's flags are not SA_RESTART without SA_RESETHAND");
	if ( !sigismember(&sa.sa_mask, SIGUSR1) )
		errx(1, "bsd_signal's mask does not have the signal");
	if ( bsd_signal(SIGUSR1, SIG_IGN) != handler )
		errx(1, "bsd_signal did not return the handler");
	if ( bsd_signal(SIGKILL, handler) != SIG_ERR )
		errx(1, "bsd_signal(SIGKILL) was not SIG_ERR");
	return 0;
}
