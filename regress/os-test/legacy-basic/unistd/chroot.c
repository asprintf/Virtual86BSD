/*[SUSv2]*/
/* Test whether chroot makes a directory the root, or is refused without privileges. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	char dir[] = "/tmp/lb.chroot.XXXXXX";
	if ( !mkdtemp(dir) )
		err(1, "mkdtemp");
	char mark[64];
	snprintf(mark, sizeof mark, "%s/mark", dir);
	FILE* fp = fopen(mark, "w");
	if ( !fp )
		err(1, "%s", mark);
	fclose(fp);
	/* in a child, so that the parent can clean up */
	pid_t pid = fork();
	if ( pid < 0 )
		err(1, "fork");
	if ( pid == 0 )
	{
		if ( chroot(dir) != 0 )
			_exit(errno == EPERM && geteuid() != 0 ? 0 : 2);
		struct stat st;
		_exit(stat("/mark", &st) == 0 && stat("/../mark", &st) == 0 ? 0 : 3);
	}
	int status;
	if ( waitpid(pid, &status, 0) != pid )
		err(1, "waitpid");
	unlink(mark);
	rmdir(dir);
	if ( !WIFEXITED(status) || WEXITSTATUS(status) == 2 )
		errx(1, "chroot failed");
	if ( WEXITSTATUS(status) == 3 )
		errx(1, "after chroot, /mark (and /../mark) is not the file");
	return 0;
}
