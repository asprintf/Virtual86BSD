/*[SUSv4]*/
/* Test whether setpgrp makes a process group leader, and does nothing to a session leader. */

#include <sys/wait.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int st;
	pid_t pid = fork();
	if ( pid == 0 )
	{
		pid_t g = setpgrp();
		_exit(g == getpid() && getpgrp() == getpid() ? 0 : 1);
	}
	if ( waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0 )
		errx(1, "setpgrp did not make the child a group leader");
	pid = fork();
	if ( pid == 0 )
	{
		setsid();
		pid_t g = setpgrp();
		_exit(g == getpid() && getpgrp() == g && getsid(0) == getpid() ? 0 : 1);
	}
	if ( waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0 )
		errx(1, "setpgrp in a session leader was not without effect");
	return 0;
}
