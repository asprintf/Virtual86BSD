/* Time conversions and strftime against glibc, in many zones. */
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE  /* glibc: tm_gmtoff, tm_zone */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

static const char *zones[] = {
	"UTC", "UTC0", ":UTC", "EST5EDT,M3.2.0,M11.1.0", "CET-1CEST,M3.5.0,M10.5.0/3",
	"Europe/Amsterdam", "America/New_York", "Australia/Sydney", "Asia/Kolkata", "Pacific/Chatham",
	"America/Sao_Paulo", "Europe/Dublin", "Africa/Casablanca", "America/St_Johns", "Asia/Kathmandu",
	"<+0330>-3:30", "NZST-12NZDT,M9.5.0,M4.1.0/3", "AAA3BBB,J60/2,J300/2", "AAA-2BBB,0/1,200/-1",
	"XXX4YYY,M3.2.0/-1,M11.1.0/26", "AAA-10BBB-11,M10.1.0,M4.1.0/3", "Europe/London", "America/Los_Angeles",
	"Antarctica/Troll", "Asia/Tokyo", "WAT-1",
};
static const char *fmts[] = {
	"%a %A %b %B %c %C %d %D %e %F %g %G %h %H %I %j %m %M %n %p %r %R %S %t %T %u %U %V %w %W %x %X %y %Y %z %Z %%",
	"%s", "%Ec %EC %Ex %EX %Ey %EY %Od %Oe %OH %OI %Om %OM %OS %Ou %OU %OV %Ow %OW %Oy",
	"%010F|%04C|%06G|%012F|%08Y",
};

static int isfile(const char *z) { return strchr(z, '/') && !strchr(z, ','); }

static void showtm(const struct tm *tm)
{
	oi(tm->tm_year); oc('-'); oi(tm->tm_mon); oc('-'); oi(tm->tm_mday); oc(' ');
	oi(tm->tm_hour); oc(':'); oi(tm->tm_min); oc(':'); oi(tm->tm_sec);
	os(" w"); oi(tm->tm_wday); os(" y"); oi(tm->tm_yday); os(" d"); oi(tm->tm_isdst);
	os(" g"); oi(tm->tm_gmtoff); oc(' '); os(tm->tm_zone);
}

int main(void)
{
	size_t z, f;
	int i;
	char buf[512];
	for (z = 0; z < sizeof zones / sizeof *zones; z++) {
		setenv("TZ", zones[z], 1);
		tzset();
		/* For zone files, glibc takes tzname/daylight from the whole history,
		 * this library from the current rules: compare only for TZ strings. */
		os("TZ="); os(zones[z]);
		if (!isfile(zones[z])) {
			os(" tzname="); os(tzname[0]); oc('/'); os(tzname[1]);
			os(" timezone="); oi(timezone); os(" daylight="); oi(daylight);
		}
		onl();
		for (i = 0; i < 400; i++) {
			struct tm tm, tm2, *p;
			time_t t, t2;
			/* glibc applies TZ-string rules only from 1970 on; POSIX has no such limit */
			if (i < 300) t = (time_t)(rng() % 7000000000ULL) - (isfile(zones[z]) ? 2500000000LL : 0);
			else { /* near transitions: hours around the changes of 2024 */
				unsigned long long d = rng(), h = rng();
				t = 1704067200 + (time_t)(d % 366) * 86400 + (time_t)(h % 48) * 1800;
			}
			p = localtime_r(&t, &tm);
			if (!p) { os("null\n"); continue; }
			oi(t); oc(' '); showtm(&tm);
			tm2 = tm; t2 = mktime(&tm2);
			os(" mk="); oi(t2 - t);
			tm2 = tm; tm2.tm_isdst = -1; tm2.tm_mday += 40; tm2.tm_hour -= 30; tm2.tm_sec += 4000;
			t2 = mktime(&tm2);
			/* after 2038 glibc's mktime misreads some times just after a
			 * spring-forward change (e.g. 2108-03-11 03:39:51 EST5EDT) */
			if (t < 2147483647) { os(" norm="); oi(t2); oc(' '); showtm(&tm2); }
			for (f = 0; f < sizeof fmts / sizeof *fmts; f++) {
				size_t n = strftime(buf, sizeof buf, fmts[f], &tm);
				os(" |"); oi((long long)n); oc(':'); os(buf);
			}
			onl();
		}
	}
	/* gmtime over a wide range, and strftime with a small buffer */
	setenv("TZ", "UTC", 1); tzset();
	for (i = 0; i < 3000; i++) {
		long long r = (long long)(rng() >> 6) - (1LL << 57);
		time_t t = i < 1500 ? (time_t)r : (time_t)(rng() % 400000000000ULL) - 200000000000LL;
		struct tm tm;
		if (!gmtime_r(&t, &tm)) { os("overflow\n"); continue; }
		tm.tm_zone = "";   /* glibc says "GMT", this library "UTC" */
		showtm(&tm); oc(' ');
		/* %C before 1000: POSIX wants [00,99] and truncation, glibc neither */
		strftime(buf, sizeof buf, tm.tm_year >= -900 ? "%Y %G %V %U %W %j %C %y %g %F" : "%Y %G %V %U %W %j %y %g %F", &tm);
		os(buf); onl();
	}
	{ struct tm tm; time_t t = 0; gmtime_r(&t, &tm);
	  oi((long long)strftime(buf, 5, "%Y-%m", &tm)); oi((long long)strftime(buf, 8, "%Y-%m", &tm)); os(buf);
	  os(asctime(&tm)); t = 1234567890; os(ctime(&t)); onl(); }
	oflush();
	return 0;
}
