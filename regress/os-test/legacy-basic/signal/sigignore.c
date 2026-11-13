/*[SUSv4]*/
/* Test whether sigignore sets SIG_IGN, and refuses SIGKILL and SIGSTOP. */

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	struct sigaction sa;
	if ( sigignore(SIGUSR2) != 0 )
		err(1, "sigignore");
	if ( sigaction(SIGUSR2, NULL, &sa) != 0 || sa.sa_handler != SIG_IGN )
		errx(1, "sigignore did not set SIG_IGN");
	kill(getpid(), SIGUSR2);	/* nothing happens */
	errno = 0;
	if ( sigignore(SIGKILL) != -1 || errno != EINVAL )
		errx(1, "sigignore(SIGKILL) was not EINVAL");
	errno = 0;
	if ( sigignore(SIGSTOP) != -1 || errno != EINVAL )
		errx(1, "sigignore(SIGSTOP) was not EINVAL");
	errno = 0;
	if ( sigignore(999) != -1 || errno != EINVAL )
		errx(1, "sigignore(999) was not EINVAL");
	return 0;
}
