/*[SUSv4]*/
/* Test whether sighold blocks a signal and sigrelse lets it in. */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "../basic.h"

static volatile sig_atomic_t got, masked_in_handler;

static void on_sig(int s)
{
	sigset_t m;
	got++;
	sigprocmask(SIG_BLOCK, NULL, &m);
	masked_in_handler = sigismember(&m, s);
}

static int blocked(int sig)
{
	sigset_t m;
	sigprocmask(SIG_BLOCK, NULL, &m);
	return sigismember(&m, sig);
}

int main(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGUSR1, &sa, NULL);
	if ( sighold(SIGUSR1) != 0 || !blocked(SIGUSR1) )
		errx(1, "sighold did not block SIGUSR1");
	kill(getpid(), SIGUSR1);
	if ( got != 0 )
		errx(1, "a held signal was delivered");
	sigset_t pend;
	sigpending(&pend);
	if ( sigismember(&pend, SIGUSR1) != 1 )
		errx(1, "the held signal is not pending");
	if ( sigrelse(SIGUSR1) != 0 || blocked(SIGUSR1) )
		errx(1, "sigrelse did not unblock SIGUSR1");
	if ( got != 1 )
		errx(1, "the pending signal did not arrive on sigrelse");
	errno = 0;
	if ( sighold(0) != -1 || errno != EINVAL )
		errx(1, "sighold(0) was not EINVAL");
	errno = 0;
	if ( sighold(999) != -1 || errno != EINVAL )
		errx(1, "sighold(999) was not EINVAL");
	errno = 0;
	if ( sigrelse(-1) != -1 || errno != EINVAL )
		errx(1, "sigrelse(-1) was not EINVAL");
	return 0;
}
