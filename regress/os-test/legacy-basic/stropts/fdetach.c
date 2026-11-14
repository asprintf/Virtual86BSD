/*[SUSv4]*/
/* Test whether fdetach fails with ENOENT for a missing file and EINVAL for a file with nothing attached. */

#include <errno.h>
#include <stropts.h>

#include "../basic.h"

int main(void)
{
	errno = 0;
	if ( fdetach("/nonexistent/file") != -1 || errno != ENOENT )
		errx(1, "fdetach of a missing file did not fail with ENOENT");
	errno = 0;
	if ( fdetach("/") != -1 || errno != EINVAL )
		errx(1, "fdetach of a file with nothing attached did not fail with EINVAL");
	return 0;
}
