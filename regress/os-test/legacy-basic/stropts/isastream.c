/*[SUSv4]*/
/* Test whether isastream gives 0 for a file that is not a STREAM, and fails for a bad descriptor. */

#include <errno.h>
#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int fds[2];
	if ( pipe(fds) < 0 )
		err(1, "pipe");
	if ( isastream(fds[0]) != 0 )
		errx(1, "isastream of a pipe is not 0");
	int null = open("/dev/null", 0);
	if ( null < 0 )
		err(1, "/dev/null");
	if ( isastream(null) != 0 )
		errx(1, "isastream of /dev/null is not 0");
	errno = 0;
	if ( isastream(-1) != -1 || errno != EBADF )
		errx(1, "isastream(-1) did not fail with EBADF");
	return 0;
}
