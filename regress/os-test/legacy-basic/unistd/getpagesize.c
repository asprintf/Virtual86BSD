/*[SUSv2]*/
/* Test whether getpagesize is sysconf(_SC_PAGESIZE). */

#include <unistd.h>

#include "../basic.h"

int main(void)
{
	if ( getpagesize() != sysconf(_SC_PAGESIZE) )
		errx(1, "getpagesize was %d, sysconf %ld", getpagesize(), sysconf(_SC_PAGESIZE));
	return 0;
}
