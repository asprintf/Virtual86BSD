/*[SUSv4]*/
/* Test whether fattach fails with EBADF for a bad descriptor and EINVAL for a file that is not a STREAM. */

#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int fds[2];
	if ( pipe(fds) < 0 )
		err(1, "pipe");
	errno = 0;
	if ( fattach(-1, "/tmp") != -1 || errno != EBADF )
		errx(1, "fattach(-1) did not fail with EBADF");
	errno = 0;
	if ( fattach(fds[0], "/tmp") != -1 || errno != EINVAL )
		errx(1, "fattach of a pipe did not fail with EINVAL");
	return 0;
}
