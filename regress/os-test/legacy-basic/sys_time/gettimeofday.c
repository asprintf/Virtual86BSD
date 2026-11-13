/*[SUSv4]*/
/* Test whether gettimeofday gives the time in seconds and microseconds. */

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	struct timeval a, b;
	struct timespec ts;
	int tz;
	if ( gettimeofday(&a, NULL) != 0 )
		err(1, "gettimeofday");
	clock_gettime(CLOCK_REALTIME, &ts);
	if ( a.tv_usec < 0 || a.tv_usec >= 1000000 )
		errx(1, "tv_usec is %ld", (long) a.tv_usec);
	if ( a.tv_sec < ts.tv_sec - 1 || a.tv_sec > ts.tv_sec + 1 )
		errx(1, "gettimeofday is not the time clock_gettime gives");
	/* a tzp that is not null: unspecified, left alone */
	if ( gettimeofday(&a, &tz) != 0 )
		err(1, "gettimeofday with tzp");
	usleep(30000);
	if ( gettimeofday(&b, NULL) != 0 )
		err(1, "gettimeofday");
	long long dt = (b.tv_sec - a.tv_sec) * 1000000LL + (b.tv_usec - a.tv_usec);
	if ( dt < 25000 || dt >= 1000000 )
		errx(1, "30 ms of usleep were %lld microseconds of gettimeofday", dt);
	return 0;
}
