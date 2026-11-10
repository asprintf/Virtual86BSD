/* <time.h> behaviour that has no glibc reference: POSIX.1-2024 strftime
 * flags, getdate, timers and clocks; strptime's E and O modifiers where
 * glibc fails (no eras or alternative digits: the plain conversions). */
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "out.h"

static volatile sig_atomic_t fired;
static void on_alarm(int s) { (void)s; fired++; }

static int sf(const char *fmt, int year, const char *want)
{
	char buf[64];
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	tm.tm_year = year - 1900; tm.tm_mon = 2; tm.tm_mday = 5; tm.tm_wday = 0; tm.tm_yday = 63;
	strftime(buf, sizeof buf, fmt, &tm);
	if (strcmp(buf, want)) { os("strftime "); os(fmt); os(" -> "); os(buf); os(" want "); os(want); onl(); return 0; }
	return 1;
}

/* strptime of all of in: the year (tm_year), or -9999 */
static int py(const char *f, const char *in, struct tm *tm)
{
	char *e;
	memset(tm, 0, sizeof *tm);
	e = strptime(in, f, tm);
	return e && !*e ? tm->tm_year : -9999;
}

int main(void)
{
	struct timespec a, b;
	struct tm tm, *p;
	time_t t;
	char buf[128];
	timer_t tid;
	struct itimerspec its;
	struct sigevent sev;
	clockid_t cid;
	int fd;

	/* strftime flags and widths (XSH strftime) */
	CHECK(sf("%Y", 2024, "2024") && sf("%+4Y", 2024, "2024") && sf("%+5Y", 2024, "+2024"));
	CHECK(sf("%+6Y", 2024, "+02024") && sf("%06Y", 2024, "002024") && sf("%+4Y", 12345, "+12345"));
	CHECK(sf("%Y", 12345, "12345") && sf("%04Y", 12345, "12345") && sf("%Y", -44, "-44") && sf("%05Y", -44, "-0044"));
	CHECK(sf("%+10F", 2024, "2024-03-05") && sf("%+11F", 2024, "+2024-03-05") && sf("%+12F", 12345, "+12345-03-05"));
	CHECK(sf("%010F", 2024, "2024-03-05") && sf("%011F", 2024, "02024-03-05") && sf("%F", 99, "99-03-05"));
	CHECK(sf("%C", 2024, "20") && sf("%C", 324, "03") && sf("%+3C", 2024, "+20") && sf("%+2C", 12345, "+123") && sf("%03C", 2024, "020"));
	CHECK(sf("%Ey|%OH|%Ec", 2024, "24|00|Sun Mar  5 00:00:00 2024"));

	/* strptime extensions of POSIX.1-2024 */
	memset(&tm, 0, sizeof tm);
	CHECK(strptime("+2024", "%Y", &tm) && tm.tm_year == 124);
	CHECK(strptime("-44", "%Y", &tm) && tm.tm_year == -1944);
	setenv("TZ", "UTC0", 1); tzset();
	CHECK(strptime("86400", "%s", &tm) && tm.tm_mday == 2 && tm.tm_year == 70);
	setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1); tzset();
	tm.tm_isdst = -1; CHECK(strptime("EDT", "%Z", &tm) && tm.tm_isdst == 1);
	tm.tm_isdst = -1; CHECK(strptime("EST", "%Z", &tm) && tm.tm_isdst == 0);
	CHECK(strptime("-0430", "%z", &tm) && tm.tm_gmtoff == -16200);
	CHECK(!strcmp(tzname[0], "EST") && !strcmp(tzname[1], "EDT") && timezone == 18000 && daylight == 1);

	/* mktime and tm_isdst */
	memset(&tm, 0, sizeof tm);
	tm.tm_year = 124; tm.tm_mon = 6; tm.tm_mday = 1; tm.tm_hour = 12; tm.tm_isdst = -1;
	t = mktime(&tm); CHECK(tm.tm_isdst == 1 && tm.tm_gmtoff == -14400 && t == 1719849600);
	tm.tm_hour = 12; tm.tm_isdst = 0;   /* asks for standard time in summer */
	CHECK(mktime(&tm) == 1719853200 && tm.tm_hour == 13);
	memset(&tm, 0, sizeof tm);           /* a time in the spring gap moves forward */
	tm.tm_year = 124; tm.tm_mon = 2; tm.tm_mday = 10; tm.tm_hour = 2; tm.tm_min = 30; tm.tm_isdst = -1;
	CHECK(mktime(&tm) == 1710055800 && tm.tm_hour == 3 && tm.tm_isdst == 1);
	setenv("TZ", "Europe/Amsterdam", 1); tzset();
	t = 1719849600; p = localtime(&t); CHECK(p && p->tm_hour == 18 && !strcmp(p->tm_zone, "CEST"));
	setenv("TZ", "No/Such_Zone", 1); tzset();
	p = localtime(&t); CHECK(p && p->tm_hour == 16 && p->tm_gmtoff == 0);
	setenv("TZ", "../../etc/passwd", 1); tzset();
	p = localtime(&t); CHECK(p && p->tm_gmtoff == 0);
	unsetenv("TZ"); tzset();
	t = (time_t)1 << 62; CHECK(localtime(&t) == 0 && errno == EOVERFLOW);

	/* getdate */
	unsetenv("DATEMSK");
	CHECK(getdate("x") == 0 && getdate_err == 1);
	setenv("DATEMSK", "/nonexistent-datemsk", 1);
	CHECK(getdate("x") == 0 && getdate_err == 2);
	setenv("DATEMSK", "/tmp", 1);
	CHECK(getdate("x") == 0 && getdate_err == 4);
	strcpy(buf, "/tmp/libc-datemsk.XXXX");
	fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	CHECK(fd >= 0);
	if (fd >= 0) {
		const char *tpl = "%Y-%m-%d %H:%M\n%m/%d/%Y\n%A\n%B\n%H:%M\n";
		struct tm now;
		time_t tn = time(0);
		write(fd, tpl, strlen(tpl));
		close(fd);
		setenv("DATEMSK", buf, 1);
		setenv("TZ", "UTC0", 1); tzset();
		p = getdate("2001-02-03 04:05");
		CHECK(p && p->tm_year == 101 && p->tm_mon == 1 && p->tm_mday == 3 && p->tm_hour == 4 && p->tm_min == 5 && p->tm_wday == 6);
		p = getdate("  12/25/2030 ");
		CHECK(p && p->tm_year == 130 && p->tm_mon == 11 && p->tm_mday == 25);
		p = getdate("02/30/2001");
		CHECK(!p && getdate_err == 8);
		p = getdate("not a date");
		CHECK(!p && getdate_err == 7);
		localtime_r(&tn, &now);
		p = getdate("SUNDAY");     /* case-insensitive; today or later */
		CHECK(p && p->tm_wday == 0 && p->tm_yday >= now.tm_yday - 0 - (p->tm_year > now.tm_year) * 400);
		p = getdate("January");
		CHECK(p && p->tm_mon == 0 && p->tm_mday == 1 && p->tm_year == now.tm_year + (now.tm_mon > 0));
		unlink(buf);
	}
	unsetenv("TZ"); tzset();

	/* clocks */
	CHECK(clock_gettime(CLOCK_MONOTONIC, &a) == 0 && clock_getres(CLOCK_REALTIME, &b) == 0 && b.tv_nsec > 0);
	b.tv_sec = 0; b.tv_nsec = 20000000;
	CHECK(nanosleep(&b, 0) == 0);
	clock_gettime(CLOCK_MONOTONIC, &b);
	CHECK((b.tv_sec - a.tv_sec) * 1000000000L + b.tv_nsec - a.tv_nsec >= 20000000);
	b.tv_nsec += 10000000; if (b.tv_nsec >= 1000000000) { b.tv_sec++; b.tv_nsec -= 1000000000; }
	CHECK(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &b, 0) == 0);
	CHECK(clock_nanosleep(CLOCK_THREAD_CPUTIME_ID, 0, &b, 0) == EINVAL);
	b.tv_nsec = 2000000000; CHECK(nanosleep(&b, 0) == -1 && errno == EINVAL);
	CHECK(clock_getcpuclockid(getpid(), &cid) == 0 && clock_gettime(cid, &a) == 0);
	CHECK(clock_getcpuclockid(999999999, &cid) == ESRCH);
	CHECK(timespec_get(&a, TIME_UTC) == TIME_UTC && timespec_get(&a, 0) == 0);
	CHECK(time(&t) == t && t > 1700000000 && difftime(t, t - 90) == 90.0 && clock() >= 0);

	/* timers */
	signal(SIGALRM, on_alarm);
	CHECK(timer_create(CLOCK_MONOTONIC, 0, &tid) == 0);
	its.it_value.tv_sec = 0; its.it_value.tv_nsec = 5000000;
	its.it_interval.tv_sec = 0; its.it_interval.tv_nsec = 5000000;
	CHECK(timer_settime(tid, 0, &its, 0) == 0);
	while (fired < 3) pause();
	CHECK(timer_gettime(tid, &its) == 0 && its.it_interval.tv_nsec == 5000000);
	CHECK(timer_getoverrun(tid) >= 0 && timer_delete(tid) == 0);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_NONE;
	CHECK(timer_create(CLOCK_REALTIME, &sev, &tid) == 0 && timer_delete(tid) == 0);
	sev.sigev_notify = 99;
	CHECK(timer_create(CLOCK_REALTIME, &sev, &tid) == -1 && errno == EINVAL);

	/* E and O without eras or alternative digits: as without them */
	{
		struct tm tm;
		CHECK(py("%Ey", "68", &tm) == 168 && py("%Ey", "69", &tm) == 69);
		CHECK(py("%EC%Ey", "1868", &tm) == -32 && py("%Ey %EC", "68 18", &tm) == -32);
		CHECK(py("%EY", "2024", &tm) == 124 && py("%EC", "19", &tm) == 0);
		CHECK(py("%Ex", "12/31/99", &tm) == 99 && tm.tm_mon == 11 && tm.tm_mday == 31);
		CHECK(py("%Od %Om %OH %OM %OS %Oy", "23 10 21 07 09 05", &tm) == 105 &&
		      tm.tm_mday == 23 && tm.tm_mon == 9 && tm.tm_hour == 21 && tm.tm_min == 7 && tm.tm_sec == 9);
		CHECK(py("%Ow %OI %Oe", "5 09 7", &tm) == 0 && tm.tm_wday == 5 && tm.tm_hour == 9 && tm.tm_mday == 7);
		/* eras of ja_JP and th_TH, if run.sh made them (a UTF-8 name
		 * without data would be C.UTF-8) */
		if (setlocale(LC_ALL, "ja_JP.UTF-8") && *nl_langinfo(ERA)) {
			CHECK(py("%EY", "平成元年", &tm) == 89);              /* the era's own format, %EC元年 */
			CHECK(py("%EY", "令和6年", &tm) == 124);
			CHECK(py("%EC%Ey年", "昭和64年", &tm) == 89);         /* its last year, 1989 */
			CHECK(py("%EC%Ey年", "昭和65年", &tm) == -9999);      /* after its end */
			CHECK(py("%EC", "大正", &tm) == 13);   /* the start of the first entry of that name (glibc too) */
			CHECK(py("%Ex", "平成31年04月30日", &tm) == 119 && tm.tm_mon == 3 && tm.tm_mday == 30);
			CHECK(py("%Od日%OH時", "二十三日九時", &tm) == 0 && tm.tm_mday == 23 && tm.tm_hour == 9);
			CHECK(py("%Om", "十二", &tm) == 0 && tm.tm_mon == 11);  /* the longest: not 十 */
			CHECK(py("%Od", "23", &tm) == 0 && tm.tm_mday == 23);   /* ASCII digits too */
		}
		if (setlocale(LC_ALL, "th_TH.UTF-8") && *nl_langinfo(ERA)) {
			CHECK(py("%EC %Ey", "พ.ศ. 2567", &tm) == 124);
			CHECK(py("%EY", "พ.ศ. 2443", &tm) == 0);
		}
		setlocale(LC_ALL, "C");
	}

	os(failures ? "time FAILED\n" : "time ok\n");
	oflush();
	return failures != 0;
}
