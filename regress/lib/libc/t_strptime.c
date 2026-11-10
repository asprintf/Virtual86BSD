/* strptime against glibc. */
#define _XOPEN_SOURCE 800
#include <time.h>
#include <string.h>
#include "out.h"

static const char *cases[][2] = {
	{ "%Y-%m-%d", "2024-02-29" }, { "%Y-%m-%d", "1999-12-31xyz" }, { "%d/%m/%Y", "1/2/3" },
	{ "%H:%M:%S", "23:59:60" }, { "%H:%M:%S", "24:00:00" }, { "%H:%M", " 7: 5" },
	{ "%a %b %e %H:%M:%S %Y", "Thu Jan  1 00:00:00 1970" }, { "%A, %B %d", "monday, FEBRUARY 03" },
	{ "%b", "Sept" }, { "%b", "sep" }, { "%B", "Jun" }, { "%a", "Satur" }, { "%A", "Wednesday" },
	{ "%y", "68" }, { "%y", "69" }, { "%y", "00" }, { "%C%y", "1999" }, { "%C", "20" }, { "%y %C", "05 19" },
	{ "%D", "12/31/99" }, { "%x %X", "01/02/03 04:05:06" }, { "%T", "12:34:56" }, { "%R", "01:02" },
	{ "%r", "12:00:00 AM" }, { "%r", "12:30:00 PM" }, { "%I %p", "01 pm" }, { "%I %p", "11 AM" },
	{ "%j", "366" }, { "%j", "001" }, { "%j", "367" }, { "%m", "13" }, { "%m", "0" }, { "%d", "32" },
	{ "%e", " 9" }, { "%n%t%Y", "   \t 2001" }, { "%%%Y", "%2001" }, { "%Y%%", "2001%" },
	{ "%w", "6" }, { "%w", "7" }, { "%u", "7" }, { "%u", "1" }, { "%F", "2024-03-15" },
	{ "%Y", "12345" }, { "%H%M%S", "123456" },
	{ "%d%b%Y", "05Jan2020" }, { "%Y-%m-%dT%H:%M:%S", "2020-01-02T03:04:05" }, { "%c", "Mon Feb  3 04:05:06 2020" },
	{ "%U %W %V", "12 13 14" }, { "%G %g", "2020 20" }, { "%EY %Od %OH", "2020 07 08" },
	{ " %Y", "2020" }, { "%Y ", "2020" }, { "%Y-%m", "2020 -01" }, { "x%Y", "X2020" },
	{ "%H", "7x" }, { "%p", "XM" },
	/* not compared: a sign before %Y (POSIX.1-2024 allows it, glibc does
	 * not); "60" for %M (glibc takes the "6") */
};

int main(void)
{
	size_t i;
	for (i = 0; i < sizeof cases / sizeof *cases; i++) {
		struct tm tm;
		char *e;
		memset(&tm, 0x55, sizeof tm);   /* untouched fields stay recognisable */
		e = strptime(cases[i][1], cases[i][0], &tm);
		os(cases[i][0]); os(" ["); os(cases[i][1]); os("] ");
		if (!e) { os("null\n"); continue; }
		oi(e - cases[i][1]); oc(':');
		oi(tm.tm_year); oc(' '); oi(tm.tm_mon); oc(' '); oi(tm.tm_mday); oc(' ');
		oi(tm.tm_hour); oc(' '); oi(tm.tm_min); oc(' '); oi(tm.tm_sec); oc(' ');
		/* the weekday and day of the year only for complete dates; glibc
		 * also computes them from garbage */
		if (tm.tm_year != 0x55555555 && tm.tm_mon != 0x55555555 && tm.tm_mday != 0x55555555)
			{ oi(tm.tm_wday); oc(' '); oi(tm.tm_yday); }
		onl();
	}
	oflush();
	return 0;
}
