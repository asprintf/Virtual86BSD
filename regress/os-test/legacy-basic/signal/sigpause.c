/*[SUSv4]*/
/* Test whether sigpause unblocks a signal, waits for it and puts the mask back. */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
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
	sigset(SIGALRM, on_sig);
	if ( sighold(SIGALRM) != 0 )
		err(1, "sighold");
	struct itimerval v;
	memset(&v, 0, sizeof v);
	v.it_value.tv_usec = 30000;
	setitimer(ITIMER_REAL, &v, NULL);
	errno = 0;
	if ( sigpause(SIGALRM) != -1 || errno != EINTR )
		errx(1, "sigpause did not return -1 with EINTR");
	if ( got != 1 )
		errx(1, "the signal did not arrive in sigpause");
	if ( !blocked(SIGALRM) )
		errx(1, "sigpause did not put the mask back");
	sigrelse(SIGALRM);
	errno = 0;
	if ( sigpause(999) != -1 || errno != EINVAL )
		errx(1, "sigpause(999) was not EINVAL");
	return 0;
}
