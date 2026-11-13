/*[SUSv4]*/
/* Test whether siginterrupt turns SA_RESTART off and on, and what that does to a read. */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
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
	sigaction(SIGALRM, &sa, NULL);
	if ( siginterrupt(SIGALRM, 0) != 0 || sigaction(SIGALRM, NULL, &sa) != 0 ||
	     !(sa.sa_flags & SA_RESTART) || sa.sa_handler != on_sig )
		errx(1, "siginterrupt(sig, 0) did not set SA_RESTART");
	if ( siginterrupt(SIGALRM, 1) != 0 || sigaction(SIGALRM, NULL, &sa) != 0 ||
	     (sa.sa_flags & SA_RESTART) )
		errx(1, "siginterrupt(sig, 1) did not clear SA_RESTART");
	errno = 0;
	if ( siginterrupt(999, 1) != -1 || errno != EINVAL )
		errx(1, "siginterrupt(999) was not EINVAL");
	for ( int flag = 1; flag >= 0; flag-- )
	{
		int fds[2];
		if ( pipe(fds) != 0 )
			err(1, "pipe");
		siginterrupt(SIGALRM, flag);
		pid_t pid = fork();
		if ( pid == 0 )
		{
			close(fds[0]);
			usleep(250000);
			write(fds[1], "x", 1);
			_exit(0);
		}
		close(fds[1]);
		got = 0;
		struct itimerval v;
		memset(&v, 0, sizeof v);
		v.it_value.tv_usec = 40000;
		setitimer(ITIMER_REAL, &v, NULL);
		char c = 0;
		errno = 0;
		ssize_t r = read(fds[0], &c, 1);
		if ( flag && (r != -1 || errno != EINTR) )
			errx(1, "with siginterrupt(sig, 1) the read was not interrupted");
		if ( !flag && (r != 1 || c != 'x') )
			errx(1, "with siginterrupt(sig, 0) the read was not restarted");
		if ( got != 1 )
			errx(1, "the signal did not arrive");
		close(fds[0]);
		int st;
		waitpid(pid, &st, 0);
	}
	return 0;
}
