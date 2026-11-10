/* strftime(): names and formats from the locale's LC_TIME. Flags '0' and
 * '+' and a minimum field width are honoured for %C, %F, %G and %Y; the
 * O modifier gives the alternative month names (%OB, %Ob) and digits; E
 * gives the era (%EC %Ey %EY) and its formats (%Ec %Ex %EX). */
#include <time.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <stdint.h>
#include "time_impl.h"
#include "locale_impl.h"
#include "era.h"

struct out { char *s; size_t n, max; const struct __lcat *t; int depth, alt; };

static int era_of(const struct out *o, const struct tm *tm, struct __era *e)
{
	unsigned i, n = __era_count(o->t);
	int32_t d[3];
	d[0] = tm->tm_year; d[1] = tm->tm_mon; d[2] = tm->tm_mday;
	for (i = 0; i < n; i++) {
		__era_get(o->t, i, e);
		if (__era_has(e, d)) return 1;
	}
	return 0;
}

static const char *alt_digit(const struct out *o, long long v) { return __alt_digit(o->t, v); }

static const char *item(struct out *o, unsigned idx, unsigned alt)
{
	const char *s = __lc_str(o->t, idx);
	if (!*s && alt) s = __lc_str(o->t, alt);   /* files without alternative names */
	return s;
}

static void put(struct out *o, const char *s, size_t n)
{
	for (; n; n--, s++) { if (o->n < o->max) o->s[o->n] = *s; o->n++; }
}

static void puts_(struct out *o, const char *s) { put(o, s, strlen(s)); }

/* v with at least width digits, padded with pad */
static void num(struct out *o, long long v, int width, char pad)
{
	const char *a;
	char t[32];
	int n = 0, neg = v < 0;
	unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
	if (o->alt && (a = alt_digit(o, v))) { puts_(o, a); return; }
	do t[n++] = (char)('0' + u % 10); while (u /= 10);
	if (pad == '0' || pad == ' ') while (n < width - neg) t[n++] = pad;
	if (neg) t[n++] = '-';
	while (n) { char c = t[--n]; put(o, &c, 1); }
}

static int is_leap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* ISO 8601 week number and week-based year */
static int iso_week(const struct tm *tm, long long *wyear)
{
	long long y = tm->tm_year + 1900LL;
	int wday = (tm->tm_wday + 6) % 7;          /* Monday = 0 */
	int week = (tm->tm_yday - wday + 10) / 7;  /* (yday+1 - (wday+1) + 10) / 7 */
	if (week < 1) {
		/* the last week of the previous year */
		int pyday = tm->tm_yday + 365 + is_leap(y - 1);
		week = (pyday - wday + 10) / 7;
		y--;
	} else if (week == 53) {
		/* week 1 of the next year unless Thursday of this week is in this year */
		int days = 365 + is_leap(y);
		if (tm->tm_yday - wday + 3 >= days) { week = 1; y++; }
	}
	*wyear = y;
	return week;
}

/* a year for %C, %F, %G, %Y with flag and width; c is the specifier */
static void year(struct out *o, long long y, char flag, int width, char c)
{
	char t[32];
	int n = 0, neg = y < 0, digits;
	unsigned long long u = neg ? 0ULL - (unsigned long long)y : (unsigned long long)y;
	int limit = c == 'C' ? 2 : 4;
	do t[n++] = (char)('0' + u % 10); while (u /= 10);
	if (c == 'C' && !flag && !width) {
		while (n < 2) t[n++] = '0';
		if (neg) t[n++] = '-';
		while (n) { char ch = t[--n]; put(o, &ch, 1); }
		return;
	}
	if (!flag && !width) {   /* plain %Y, %G */
		if (neg) t[n++] = '-';
		while (n) { char ch = t[--n]; put(o, &ch, 1); }
		return;
	}
	digits = n;
	{
		/* with '+', a sign when the field takes more than limit bytes */
		int sign = neg, total;
		if (!neg && flag == '+') sign = (digits > width ? digits : width) > limit;
		total = digits + sign;
		if (sign) { char ch = neg ? '-' : '+'; put(o, &ch, 1); }
		for (; total < width; total++) put(o, "0", 1);
		while (n) { char ch = t[--n]; put(o, &ch, 1); }
	}
}

static void conv(struct out *o, char c, char flag, int width, int mod, const struct tm *tm);

static void fmt(struct out *o, const char *f, const struct tm *tm)
{
	if (++o->depth > 4) { o->depth--; return; }   /* a locale's format naming itself */
	for (; *f; f++) {
		char flag = 0;
		int width = 0, mod = 0;
		if (*f != '%') { put(o, f, 1); continue; }
		f++;
		if (*f == '0' || *f == '+') flag = *f++;
		while ((unsigned)*f - '0' < 10) width = width * 10 + (*f++ - '0');
		if (*f == 'E' || *f == 'O') mod = *f++;
		if (!*f) { put(o, "%", 1); break; }
		conv(o, *f, flag, width, mod, tm);
	}
	o->depth--;
}

static void conv(struct out *o, char c, char flag, int width, int mod, const struct tm *tm)
{
	long long y = tm->tm_year + 1900LL, wy;
	int h12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
	struct __era e;
	const char *ef;
	o->alt = mod == 'O';
	switch (c) {
	case 'a': if ((unsigned)tm->tm_wday < 7) puts_(o, item(o, T_ABDAY + (unsigned)tm->tm_wday, 0)); else put(o, "?", 1); break;
	case 'A': if ((unsigned)tm->tm_wday < 7) puts_(o, item(o, T_DAY + (unsigned)tm->tm_wday, 0)); else put(o, "?", 1); break;
	case 'b': case 'h':
		if ((unsigned)tm->tm_mon >= 12) put(o, "?", 1);
		else if (mod == 'O') puts_(o, item(o, T_ABALTMON + (unsigned)tm->tm_mon, T_ABMON + (unsigned)tm->tm_mon));
		else puts_(o, item(o, T_ABMON + (unsigned)tm->tm_mon, 0));
		break;
	case 'B':
		if ((unsigned)tm->tm_mon >= 12) put(o, "?", 1);
		else if (mod == 'O') puts_(o, item(o, T_ALTMON + (unsigned)tm->tm_mon, T_MON + (unsigned)tm->tm_mon));
		else puts_(o, item(o, T_MON + (unsigned)tm->tm_mon, 0));
		break;
	case 'c':
		ef = mod == 'E' ? item(o, T_ERA_DTFMT, 0) : "";
		fmt(o, *ef ? ef : item(o, T_DTFMT, 0), tm);
		break;
	case 'C': {
		if (mod == 'E' && era_of(o, tm, &e)) { puts_(o, e.name); break; }
		/* "the year divided by 100 and truncated to an integer", in two
		 * digits at least (glibc: one, and rounded down) */
		if (o->alt && !flag && !width && alt_digit(o, y / 100)) num(o, y / 100, 2, '0');
		else year(o, y / 100, flag, width, 'C');
		break;
	}
	case 'd': num(o, tm->tm_mday, 2, '0'); break;
	case 'D': fmt(o, "%m/%d/%y", tm); break;
	case 'e': num(o, tm->tm_mday, 2, ' '); break;
	case 'F':
		if (flag || width) year(o, y, flag ? flag : '+', width < 6 ? 0 : width - 6, 'Y');
		else year(o, y, 0, 0, 'Y');
		fmt(o, "-%m-%d", tm);
		break;
	case 'g': iso_week(tm, &wy); num(o, (wy % 100 + 100) % 100, 2, '0'); break;
	case 'G': iso_week(tm, &wy); year(o, wy, flag, width, 'G'); break;
	case 'H': num(o, tm->tm_hour, 2, '0'); break;
	case 'I': num(o, h12, 2, '0'); break;
	case 'j': num(o, tm->tm_yday + 1, 3, '0'); break;
	case 'm': num(o, tm->tm_mon + 1, 2, '0'); break;
	case 'M': num(o, tm->tm_min, 2, '0'); break;
	case 'n': put(o, "\n", 1); break;
	case 'p': puts_(o, item(o, tm->tm_hour < 12 ? T_AM : T_PM, 0)); break;
	case 'r': {
		const char *f = item(o, T_TFMTAMPM, 0);
		fmt(o, *f ? f : "%I:%M:%S %p", tm);
		break;
	}
	case 'R': fmt(o, "%H:%M", tm); break;
	case 's': num(o, __tm_to_secs(tm) - tm->tm_gmtoff, 0, 0); break;
	case 'S': num(o, tm->tm_sec, 2, '0'); break;
	case 't': put(o, "\t", 1); break;
	case 'T': fmt(o, "%H:%M:%S", tm); break;
	case 'u': num(o, tm->tm_wday ? tm->tm_wday : 7, 1, '0'); break;
	case 'U': num(o, (tm->tm_yday + 7 - tm->tm_wday) / 7, 2, '0'); break;
	case 'V': num(o, iso_week(tm, &wy), 2, '0'); break;
	case 'w': num(o, tm->tm_wday, 1, '0'); break;
	case 'W': num(o, (tm->tm_yday + 7 - (tm->tm_wday + 6) % 7) / 7, 2, '0'); break;
	case 'x':
		ef = mod == 'E' ? item(o, T_ERA_DFMT, 0) : "";
		fmt(o, *ef ? ef : item(o, T_DFMT, 0), tm);
		break;
	case 'X':
		ef = mod == 'E' ? item(o, T_ERA_TFMT, 0) : "";
		fmt(o, *ef ? ef : item(o, T_TFMT, 0), tm);
		break;
	case 'y':
		if (mod == 'E' && era_of(o, tm, &e)) {
			/* the years from the start, counted in the era's direction */
			num(o, e.offset + (long long)(tm->tm_year - e.start[0]) * e.dir, 2, '0');
			break;
		}
		num(o, (y % 100 + 100) % 100, 2, '0');
		break;
	case 'Y':
		if (mod == 'E' && era_of(o, tm, &e)) { fmt(o, e.format, tm); break; }
		year(o, y, flag, width, 'Y');
		break;
	case 'z': {
		long off = tm->tm_gmtoff;
		if (tm->tm_isdst < 0) break;
		put(o, off < 0 ? "-" : "+", 1);
		if (off < 0) off = -off;
		num(o, off / 3600 * 100 + off / 60 % 60, 4, '0');
		break;
	}
	case 'Z':
		if (tm->tm_isdst < 0) break;
		if (tm->tm_zone) puts_(o, tm->tm_zone);
		else {
			__tz_lock(); __tz_update();
			puts_(o, __tzname[tm->tm_isdst > 0]);
			__tz_unlock();
		}
		break;
	case '%': put(o, "%", 1); break;
	default:  /* unknown: copied as it is */
		put(o, "%", 1); put(o, &c, 1); break;
	}
}

size_t __strftime_l(char *s, size_t max, const char *f, const struct tm *tm, void *loc)
{
	struct out o;
	locale_t l = loc ? (locale_t)loc : CURRENT_LOCALE;
	if (l == LC_GLOBAL_LOCALE) l = &__global_locale;
	o.s = s; o.n = 0; o.max = max;
	o.t = l->cat[LC_TIME];
	o.depth = 0;
	__tz_lock(); __tz_update(); __tz_unlock();   /* "as though strftime() called tzset()" */
	fmt(&o, f, tm);
	if (o.n >= max) return 0;   /* includes room for the terminating null */
	s[o.n] = 0;
	return o.n;
}

size_t strftime(char *__RESTRICT s, size_t max, const char *__RESTRICT f, const struct tm *__RESTRICT tm)
{
	return __strftime_l(s, max, f, tm, 0);
}
