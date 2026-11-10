#include <time.h>
#include <errno.h>
#include "time_impl.h"

struct tm *__gmtime_r(const time_t *t, struct tm *tm)
{
	if (__secs_to_tm(*t, tm)) return 0;
	tm->tm_isdst = 0;
	tm->tm_gmtoff = 0;
	tm->tm_zone = "UTC";
	return tm;
}

struct tm *__localtime_r(const time_t *t, struct tm *tm)
{
	long off;
	int dst;
	const char *abbr;
	/* keep the shift by the offset from overflowing */
	if (*t < -67768100567971200LL || *t > 67767976233316800LL) { errno = EOVERFLOW; return 0; }
	__tz_lock();
	__tz_update();
	__tz_lookup(*t, &off, &dst, &abbr);
	__tz_unlock();
	if (__secs_to_tm((long long)*t + off, tm)) return 0;
	tm->tm_isdst = dst;
	tm->tm_gmtoff = off;
	tm->tm_zone = abbr;
	return tm;
}
