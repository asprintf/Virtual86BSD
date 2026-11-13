/*[SUSv4]*/
/* Test whether utime sets the times, or the current time. */

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "../basic.h"

int main(void)
{
	char path[] = "/tmp/lb.utime.XXXXXX";
	int fd = mkstemp(path);
	if ( fd < 0 )
		err(1, "mkstemp");
	close(fd);
	struct utimbuf ub = { .actime = 1000000000, .modtime = 1234567890 };
	if ( utime(path, &ub) != 0 )
		err(1, "utime");
	struct stat st;
	if ( stat(path, &st) < 0 )
		err(1, "stat");
	if ( st.st_atime != 1000000000 || st.st_mtime != 1234567890 )
		errx(1, "utime set %ld %ld", (long) st.st_atime, (long) st.st_mtime);
	time_t before = time(NULL);
	if ( utime(path, NULL) != 0 )
		err(1, "utime(path, NULL)");
	if ( stat(path, &st) < 0 )
		err(1, "stat");
	if ( st.st_mtime < before || st.st_mtime > time(NULL) )
		errx(1, "utime(path, NULL) did not set the current time");
	unlink(path);
	return 0;
}
