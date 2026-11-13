/*[SUSv2]*/
/* Test whether getdtablesize is the RLIMIT_NOFILE soft limit. */

#include <sys/resource.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	struct rlimit r;
	if ( getrlimit(RLIMIT_NOFILE, &r) < 0 )
		err(1, "getrlimit");
	if ( r.rlim_cur != RLIM_INFINITY && getdtablesize() != (int)r.rlim_cur )
		errx(1, "getdtablesize was %d, the limit %ld", getdtablesize(), (long)r.rlim_cur);
	return 0;
}
