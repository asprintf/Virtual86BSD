/*[SUSv3]*/
/* Test whether vfork makes a child that runs before the parent goes on. */

#include <sys/wait.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int fd[2];
	if ( pipe(fd) < 0 )
		err(1, "pipe");
	pid_t pid = vfork();
	if ( pid < 0 )
		err(1, "vfork");
	if ( pid == 0 )
	{
		write(fd[1], "c", 1);
		_exit(42);
	}
	/* the child has run to its _exit(): its byte is there already */
	char c;
	if ( read(fd[0], &c, 1) != 1 || c != 'c' )
		errx(1, "the child's byte is not in the pipe");
	int status;
	if ( waitpid(pid, &status, 0) != pid )
		err(1, "waitpid");
	if ( !WIFEXITED(status) || WEXITSTATUS(status) != 42 )
		errx(1, "the child did not exit with 42");
	return 0;
}
