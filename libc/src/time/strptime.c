/* strptime(), names and formats from the locale's LC_TIME, and the
 * parser shared with getdate().
 * Numbers take at most the digits of their field ({2} mostly, {4} for %Y
 * unless a field width is given) and a leading sign where allowed.
 * Names match case-insensitively, full or abbreviated. */
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "time_impl.h"
#include "strptime_impl.h"
#include "locale_impl.h"

static int lower(int c) { return (unsigned)c - 'A' < 26 ? c | 32 : c; }

/* the longest of the locale's names that s starts with (without regard
 * to the case of ASCII letters): lists of n names at the given LC_TIME
 * elements; sets *which, returns the length or 0 */
static size_t names(const char *s, const unsigned *lists, int nlists, int n, int *which)
{
	const struct __lcat *t = CURRENT_LOCALE->cat[LC_TIME];
	size_t best = 0, l, i;
	int k, j;
	for (k = 0; k < nlists; k++)
		for (j = 0; j < n; j++) {
			const char *nm = __lc_str(t, lists[k] + (unsigned)j);
			l = strlen(nm);
			if (!l || l <= best) continue;
			for (i = 0; i < l && lower((unsigned char)s[i]) == lower((unsigned char)nm[i]); i++);
			if (i == l) { best = l; *which = j; }
		}
	return best;
}

static const char *timefmt(unsigned idx, const char *dflt)
{
	const char *f = __lc_str(CURRENT_LOCALE->cat[LC_TIME], idx);
	return *f ? f : dflt;
}

static const char *parse(const char *s, const char *f, struct tm *tm, int *have, int nocase, int depth);

const char *__strptime(const char *s, const char *f, struct tm *tm, int *have, int nocase)
{
	return parse(s, f, tm, have, nocase, 0);
}

/* up to maxd digits, optional sign if sign allowed; 0 on success */
static int number(const char **ps, int maxd, int sign, long long *v)
{
	const char *s = *ps;
	int neg = 0, n = 0;
	long long x = 0;
	if (sign && (*s == '+' || *s == '-')) neg = *s++ == '-';
	while (n < maxd && (unsigned)*s - '0' < 10) {
		if (x > (LLONG_MAX - 9) / 10) return -1;
		x = x * 10 + (*s++ - '0');
		n++;
	}
	if (!n) return -1;
	*v = neg ? -x : x;
	*ps = s;
	return 0;
}

static const char *parse(const char *s, const char *f, struct tm *tm, int *have, int nocase, int depth)
{
	int century = -1, yy = -1, pm = -1, h12 = -1;
	long long v;
	*have = 0;

	for (; *f; f++) {
		int width = 0;
		char c;
		if (isspace((unsigned char)*f)) {
			while (isspace((unsigned char)*s)) s++;
			continue;
		}
		if (*f != '%') {
			if (*s != *f && !(nocase && lower((unsigned char)*s) == lower((unsigned char)*f))) return 0;
			s++;
			continue;
		}
		f++;
		if (*f == '+' || *f == '0') f++;
		while ((unsigned)*f - '0' < 10) width = width * 10 + (*f++ - '0');
		if (*f == 'E' || *f == 'O') f++;
		c = *f;
		if (!c) return 0;
		/* numeric conversions skip leading white space */
		if (strchr("CdeDFgGHIjmMRsSTuUVwWyYz", c))
			while (isspace((unsigned char)*s)) s++;
		switch (c) {
		case 'a': case 'A': {
			static const unsigned l[] = { 7, 0 };   /* DAY_1, ABDAY_1 */
			int i = 0;
			size_t n = names(s, l, 2, 7, &i);
			if (!n) return 0;
			tm->tm_wday = i; s += n; *have |= HAVE_WDAY;
			break;
		}
		case 'b': case 'B': case 'h': {
			static const unsigned l[] = { 26, 14, 111, 135 };   /* MON_1, ABMON_1, ALTMON_1, ABALTMON_1 */
			int i = 0;
			size_t n = names(s, l, 4, 12, &i);
			if (!n) return 0;
			tm->tm_mon = i; s += n; *have |= HAVE_MON;
			break;
		}
		case 'c': {
			int h;
			if (depth > 3) return 0;
			s = parse(s, timefmt(40, "%a %b %e %H:%M:%S %Y"), tm, &h, nocase, depth + 1);
			if (!s) return 0;
			*have |= h;
			break;
		}
		case 'C':
			if (number(&s, width ? width : 2, 1, &v)) return 0;
			century = (int)v; *have |= HAVE_YEAR;
			break;
		case 'd': case 'e':
			if (number(&s, 2, 0, &v) || v < 1 || v > 31) return 0;
			tm->tm_mday = (int)v; *have |= HAVE_MDAY;
			break;
		case 'D': {
			int h;
			if (!(s = parse(s, "%m/%d/%y", tm, &h, nocase, depth + 1))) return 0;
			*have |= h;
			if (h & HAVE_YEAR) { yy = (tm->tm_year + 1900) % 100; century = -1; }
			break;
		}
		case 'F': {
			const char *e = width ? s + width : 0, *b = s;
			if (number(&s, width ? width : 18, 1, &v) || *s != '-') return 0;
			if (v - 1900 < INT_MIN || v - 1900 > INT_MAX) return 0;
			tm->tm_year = (int)(v - 1900); century = yy = -1; s++;
			if (number(&s, 2, 0, &v) || v < 1 || v > 12 || *s != '-') return 0;
			tm->tm_mon = (int)v - 1; s++;
			if (number(&s, 2, 0, &v) || v < 1 || v > 31) return 0;
			tm->tm_mday = (int)v;
			if (e && s > e) return 0;
			(void)b;
			*have |= HAVE_YEAR | HAVE_MON | HAVE_MDAY | HAVE_FULLYEAR;
			break;
		}
		case 'g':
			if (number(&s, 2, 1, &v)) return 0;
			break;
		case 'G':
			if (number(&s, width ? width : 4, 1, &v)) return 0;
			break;
		case 'H':
			if (number(&s, 2, 0, &v) || v > 23) return 0;
			tm->tm_hour = (int)v; h12 = -1; *have |= HAVE_HOUR;
			break;
		case 'I':
			if (number(&s, 2, 0, &v) || v < 1 || v > 12) return 0;
			h12 = (int)v % 12; *have |= HAVE_HOUR;
			break;
		case 'j':
			if (number(&s, 3, 0, &v) || v < 1 || v > 366) return 0;
			tm->tm_yday = (int)v - 1; *have |= HAVE_YDAY;
			break;
		case 'm':
			if (number(&s, 2, 0, &v) || v < 1 || v > 12) return 0;
			tm->tm_mon = (int)v - 1; *have |= HAVE_MON;
			break;
		case 'M':
			if (number(&s, 2, 0, &v) || v > 59) return 0;
			tm->tm_min = (int)v; *have |= HAVE_MIN;
			break;
		case 'n': case 't':
			while (isspace((unsigned char)*s)) s++;
			break;
		case 'p': {
			static const unsigned l[] = { 38 };   /* AM_STR, PM_STR */
			int i = 0;
			size_t n = names(s, l, 1, 2, &i);
			if (!n) return 0;
			pm = i;
			s += n;
			break;
		}
		case 'r': case 'R': case 'T': case 'X': {
			int h;
			const char *sub = c == 'r' ? timefmt(43, "%I:%M:%S %p") : c == 'R' ? "%H:%M" :
			                  c == 'X' ? timefmt(42, "%H:%M:%S") : "%H:%M:%S";
			if (depth > 3) return 0;
			if (!(s = parse(s, sub, tm, &h, nocase, depth + 1))) return 0;
			*have |= h;
			h12 = -1; pm = -1;   /* already applied by the inner call */
			break;
		}
		case 's': {
			time_t t;
			if (number(&s, 19, 1, &v)) return 0;
			t = (time_t)v;
			if (!__localtime_r(&t, tm)) return 0;
			*have |= HAVE_YEAR | HAVE_MON | HAVE_MDAY | HAVE_HOUR | HAVE_MIN | HAVE_SEC | HAVE_FULLYEAR;
			century = yy = -1;
			break;
		}
		case 'S':
			if (number(&s, 2, 0, &v) || v > 60) return 0;
			tm->tm_sec = (int)v; *have |= HAVE_SEC;
			break;
		case 'u':
			if (number(&s, 1, 0, &v) || v < 1 || v > 7) return 0;
			tm->tm_wday = (int)v % 7; *have |= HAVE_WDAY;
			break;
		case 'U': case 'W': case 'V':
			if (number(&s, 2, 0, &v) || v > 53) return 0;
			break;
		case 'w':
			if (number(&s, 1, 0, &v) || v > 6) return 0;
			tm->tm_wday = (int)v; *have |= HAVE_WDAY;
			break;
		case 'x': {
			int h;
			if (depth > 3) return 0;
			if (!(s = parse(s, timefmt(41, "%m/%d/%y"), tm, &h, nocase, depth + 1))) return 0;
			*have |= h;
			if (h & HAVE_YEAR) { yy = (tm->tm_year + 1900) % 100; century = -1; }
			break;
		}
		case 'y':
			if (number(&s, 2, 1, &v) || v < 0 || v > 99) return 0;
			yy = (int)v; *have |= HAVE_YEAR;
			break;
		case 'Y':
			if (number(&s, width ? width : 4, 1, &v)) return 0;
			tm->tm_year = (int)(v - 1900); century = yy = -1; *have |= HAVE_YEAR | HAVE_FULLYEAR;
			break;
		case 'z': {
			int neg;
			if (*s != '+' && *s != '-') return 0;
			neg = *s++ == '-';
			if (number(&s, 2, 0, &v)) return 0;
			{
				long off = (long)v * 3600;
				if (*s == ':') s++;
				if (number(&s, 2, 0, &v)) return 0;
				off += (long)v * 60;
				tm->tm_gmtoff = neg ? -off : off;
			}
			break;
		}
		case 'Z': {
			size_t n0, n1;
			__tz_lock();
			__tz_update();
			n0 = strlen(__tzname[0]);
			n1 = strlen(__tzname[1]);
			if (strcmp(__tzname[0], __tzname[1]) && n1 && !strncmp(s, __tzname[1], n1)) {
				tm->tm_isdst = 1; s += n1;
			} else if (n0 && !strncmp(s, __tzname[0], n0)) {
				tm->tm_isdst = 0; s += n0;
			} else if (!strncmp(s, "UTC", 3) || !strncmp(s, "GMT", 3)) {
				tm->tm_isdst = 0; s += 3;
			} else {
				__tz_unlock();
				return 0;
			}
			__tz_unlock();
			*have |= HAVE_ZONE;
			break;
		}
		case '%':
			if (*s++ != '%') return 0;
			break;
		default:
			return 0;
		}
	}
	if (century >= 0 || yy >= 0) {
		int y;
		if (century >= 0) y = century * 100 + (yy >= 0 ? yy : 0);
		else y = yy < 69 ? 2000 + yy : 1900 + yy;
		tm->tm_year = y - 1900;
	}
	/* with a complete date, also give the weekday and the day of the
	 * year when they were not parsed (as other implementations do) */
	if ((*have & (HAVE_YEAR | HAVE_MON | HAVE_MDAY)) == (HAVE_YEAR | HAVE_MON | HAVE_MDAY) &&
	    (unsigned)tm->tm_mon < 12 && tm->tm_mday >= 1 && tm->tm_mday <= 31) {
		long long y = tm->tm_year + 1900LL;
		long long d = __days_from_civil(y, (unsigned)tm->tm_mon + 1, (unsigned)tm->tm_mday);
		if (!(*have & HAVE_WDAY)) tm->tm_wday = (int)((d % 7 + 11) % 7);
		if (!(*have & HAVE_YDAY)) tm->tm_yday = (int)(d - __days_from_civil(y, 1, 1));
	}
	if (h12 >= 0 && pm >= 0) tm->tm_hour = h12 + 12 * pm;
	else if (h12 >= 0) tm->tm_hour = h12;
	else if (pm >= 0 && (*have & HAVE_HOUR)) tm->tm_hour = tm->tm_hour % 12 + 12 * pm;
	return s;
}

char *strptime(const char *__RESTRICT s, const char *__RESTRICT f, struct tm *__RESTRICT tm)
{
	int have;
	return (char *)__strptime(s, f, tm, &have, 0);
}
