/*[SUSv4]*/
/* Test whether ctime_r is asctime_r of localtime_r. */

#include <string.h>
#include <time.h>

#include "../basic.h"

int main(void)
{
	time_t t = 946684799;
	struct tm tm;
	char a[26], b[26];
	if ( !localtime_r(&t, &tm) || !asctime_r(&tm, a) )
		errx(1, "localtime_r or asctime_r failed");
	if ( ctime_r(&t, b) != b )
		errx(1, "ctime_r did not return buf");
	if ( strcmp(a, b) != 0 )
		errx(1, "ctime_r gave %s, asctime_r of localtime_r %s", b, a);
	return 0;
}
