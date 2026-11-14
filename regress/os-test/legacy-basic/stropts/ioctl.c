/*[SUSv4]*/
/* Test whether a STREAMS ioctl on a file that is not a STREAM fails with ENOTTY. */

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
	if ( ioctl(fds[0], I_FIND, "ldterm") != -1 || errno != ENOTTY )
		errx(1, "I_FIND on a pipe did not fail with ENOTTY");
	errno = 0;
	if ( ioctl(fds[0], I_PUSH, "ldterm") != -1 || errno != ENOTTY )
		errx(1, "I_PUSH on a pipe did not fail with ENOTTY");
	errno = 0;
	if ( ioctl(fds[1], I_NREAD, 0) != -1 || errno != ENOTTY )
		errx(1, "I_NREAD on a pipe did not fail with ENOTTY");
	return 0;
}
