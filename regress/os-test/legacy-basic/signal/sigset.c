/*[SUSv4]*/
/* Test whether sigset sets dispositions, holds a signal and returns SIG_HOLD for a held one. */

#include <errno.h>
#include <signal.h>
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
	sigignore(SIGUSR2);
	if ( sigset(SIGUSR2, on_sig) != SIG_IGN || sigset(SIGUSR2, SIG_DFL) != on_sig ||
	     sigset(SIGUSR2, SIG_DFL) != SIG_DFL || sigset(SIGUSR2, SIG_IGN) != SIG_DFL ||
	     sigset(SIGUSR2, SIG_DFL) != SIG_IGN )
		errx(1, "sigset did not return the old dispositions");
	sigset(SIGUSR1, on_sig);
	if ( sigset(SIGUSR1, SIG_HOLD) != on_sig || !blocked(SIGUSR1) )
		errx(1, "sigset(SIG_HOLD) did not block and return the handler");
	if ( sigaction(SIGUSR1, NULL, &sa) != 0 || sa.sa_handler != on_sig )
		errx(1, "sigset(SIG_HOLD) changed the handler");
	kill(getpid(), SIGUSR1);
	if ( got != 0 )
		errx(1, "a held signal was delivered");
	if ( sigset(SIGUSR1, on_sig) != SIG_HOLD )
		errx(1, "sigset of a held signal did not return SIG_HOLD");
	if ( blocked(SIGUSR1) || got != 1 )
		errx(1, "sigset did not unblock the signal and let it in");
	if ( masked_in_handler != 1 || blocked(SIGUSR1) )
		errx(1, "the handler did not run with the signal blocked, or the mask stayed");
	errno = 0;
	if ( sigset(SIGKILL, on_sig) != SIG_ERR || errno != EINVAL )
		errx(1, "sigset(SIGKILL) was not SIG_ERR with EINVAL");
	errno = 0;
	if ( sigset(SIGSTOP, SIG_IGN) != SIG_ERR || errno != EINVAL )
		errx(1, "sigset(SIGSTOP) was not SIG_ERR with EINVAL");
	errno = 0;
	if ( sigset(999, on_sig) != SIG_ERR || errno != EINVAL )
		errx(1, "sigset(999) was not SIG_ERR with EINVAL");
	errno = 0;
	if ( sigset(0, SIG_HOLD) != SIG_ERR || errno != EINVAL )
		errx(1, "sigset(0) was not SIG_ERR with EINVAL");
	return 0;
}
