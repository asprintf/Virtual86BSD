/*[SUSv3]*/
/* Test whether sysconf and confstr answer the names of older editions. */

#include <limits.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	if ( sysconf(_SC_XOPEN_LEGACY) != _XOPEN_LEGACY )
		errx(1, "sysconf(_SC_XOPEN_LEGACY) is not _XOPEN_LEGACY");
	if ( sysconf(_SC_XBS5_LP64_OFF64) != _XBS5_LP64_OFF64 )
		errx(1, "sysconf(_SC_XBS5_LP64_OFF64) is not _XBS5_LP64_OFF64");
	if ( sysconf(_SC_XBS5_ILP32_OFF32) != _XBS5_ILP32_OFF32 )
		errx(1, "sysconf(_SC_XBS5_ILP32_OFF32) is not _XBS5_ILP32_OFF32");
	if ( sysconf(_SC_PASS_MAX) != PASS_MAX )
		errx(1, "sysconf(_SC_PASS_MAX) is not PASS_MAX");
	if ( sysconf(_SC_2_C_VERSION) != _POSIX2_C_VERSION )
		errx(1, "sysconf(_SC_2_C_VERSION) is not _POSIX2_C_VERSION");
	if ( sysconf(_SC_XOPEN_XCU_VERSION) != _XOPEN_XCU_VERSION )
		errx(1, "sysconf(_SC_XOPEN_XCU_VERSION) is not _XOPEN_XCU_VERSION");
	char buf[8];
	if ( confstr(_CS_XBS5_LP64_OFF64_CFLAGS, buf, sizeof buf) == 0 )
		errx(1, "confstr(_CS_XBS5_LP64_OFF64_CFLAGS) failed");
	return 0;
}
