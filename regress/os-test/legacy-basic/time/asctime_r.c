/*[SUSv4]*/
/* Test whether asctime_r writes the asctime string into buf. */

#include <string.h>
#include <time.h>

#include "../basic.h"

int main(void)
{
	struct tm tm = { 0 };
	tm.tm_year = 99; tm.tm_mon = 11; tm.tm_mday = 31;
	tm.tm_hour = 23; tm.tm_min = 59; tm.tm_sec = 58; tm.tm_wday = 5;
	char buf[26];
	if ( asctime_r(&tm, buf) != buf )
		errx(1, "asctime_r did not return buf");
	if ( strcmp(buf, "Fri Dec 31 23:59:58 1999\n") != 0 )
		errx(1, "asctime_r gave %s", buf);
	return 0;
}
