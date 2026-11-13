/*[SUSv4]*/
/* Test whether ulimit gets and sets the file size limit in 512-byte blocks. */

#include <errno.h>
#include <sys/resource.h>
#include <ulimit.h>

#include "../basic.h"

int main(void)
{
	struct rlimit r;
	if ( getrlimit(RLIMIT_FSIZE, &r) < 0 )
		err(1, "getrlimit");
	errno = 0;
	long v = ulimit(UL_GETFSIZE);
	if ( errno != 0 )
		err(1, "ulimit(UL_GETFSIZE)");
	if ( r.rlim_cur != RLIM_INFINITY && v != (long)(r.rlim_cur / 512) )
		errx(1, "ulimit(UL_GETFSIZE) was %ld, the limit / 512 %ld", v, (long)(r.rlim_cur / 512));
	errno = 1234;
	if ( ulimit(UL_SETFSIZE, 1000L) != 1000 )
		err(1, "ulimit(UL_SETFSIZE, 1000)");
	if ( errno != 1234 )
		errx(1, "ulimit changed errno when it went well");
	if ( getrlimit(RLIMIT_FSIZE, &r) < 0 || r.rlim_cur != 512000 )
		errx(1, "the file size limit is not 512000 after UL_SETFSIZE 1000");
	if ( ulimit(UL_GETFSIZE) != 1000 )
		errx(1, "ulimit(UL_GETFSIZE) after setting 1000 was %ld", ulimit(UL_GETFSIZE));
	errno = 0;
	if ( ulimit(12345) != -1 || errno != EINVAL )
		errx(1, "ulimit with a bad cmd did not fail with EINVAL");
	return 0;
}
