/*[SUSv3]*/
/* Test whether ftime gives the seconds and milliseconds of now. */

#include <sys/timeb.h>
#include <time.h>

#include "../basic.h"

int main(void)
{
	struct timeb tb;
	time_t before = time(NULL);
	if ( ftime(&tb) != 0 )
		errx(1, "ftime did not return 0");
	time_t after = time(NULL);
	if ( tb.time < before || tb.time > after )
		errx(1, "ftime's time %ld is not between %ld and %ld",
		     (long) tb.time, (long) before, (long) after);
	if ( tb.millitm >= 1000 )
		errx(1, "ftime's millitm is %u", tb.millitm);
	return 0;
}
