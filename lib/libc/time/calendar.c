/* Proleptic Gregorian calendar arithmetic (H. Hinnant's algorithms). */
#include <time.h>
#include <limits.h>
#include <errno.h>
#include "time_impl.h"

/* days since 1970-01-01 of y-m-d (m 1..12) */
long long __days_from_civil(long long y, unsigned m, unsigned d)
{
	long long era;
	unsigned yoe, doy, doe;
	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = (unsigned)(y - era * 400);
	doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long long)doe - 719468;
}

int __secs_to_tm(long long t, struct tm *tm)
{
	long long days, secs, era, y;
	unsigned doe, yoe, doy, mp, d, m;

	/* the year must fit in an int */
	if (t < -67768040609740800LL || t > 67767976233316800LL) { errno = EOVERFLOW; return -1; }
	days = t / 86400;
	secs = t % 86400;
	if (secs < 0) { secs += 86400; days--; }
	tm->tm_hour = (int)(secs / 3600);
	tm->tm_min = (int)(secs / 60 % 60);
	tm->tm_sec = (int)(secs % 60);
	tm->tm_wday = (int)((days % 7 + 11) % 7);  /* 1970-01-01 was a Thursday */

	days += 719468;
	era = (days >= 0 ? days : days - 146096) / 146097;
	doe = (unsigned)(days - era * 146097);
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = (long long)yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	y += m <= 2;
	if (y - 1900 > INT_MAX || y - 1900 < INT_MIN) { errno = EOVERFLOW; return -1; }
	tm->tm_year = (int)(y - 1900);
	tm->tm_mon = (int)m - 1;
	tm->tm_mday = (int)d;
	tm->tm_yday = (int)(__days_from_civil(y, m, d) - __days_from_civil(y, 1, 1));
	return 0;
}

/* tm fields may be out of range; months are folded into years first */
long long __tm_to_secs(const struct tm *tm)
{
	long long year = (long long)tm->tm_year + 1900, mon = tm->tm_mon, days;
	year += mon / 12;
	mon %= 12;
	if (mon < 0) { mon += 12; year--; }
	days = __days_from_civil(year, (unsigned)mon + 1, 1) + tm->tm_mday - 1;
	return days * 86400 + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec;
}
