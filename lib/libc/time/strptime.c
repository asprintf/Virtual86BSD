/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include "era.h"

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

/* with the E modifier, the era's format if the locale has one */
static const char *erafmt(int mod, unsigned eidx, unsigned idx, const char *dflt)
{
	const char *f = mod == 'E' ? __lc_str(CURRENT_LOCALE->cat[LC_TIME], eidx) : "";
	return *f ? f : timefmt(idx, dflt);
}

static const char *parse(const char *s, const char *f, struct tm *tm, int *have, int nocase, int depth, int pin);

const char *__strptime(const char *s, const char *f, struct tm *tm, int *have, int nocase)
{
	return parse(s, f, tm, have, nocase, 0, -1);
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

/* a number; with the O modifier the locale's alternative digits first */
static int altnumber(const char **ps, int maxd, int sign, long long *v, int mod)
{
	if (mod == 'O') {
		const char *s = *ps;
		int a = __alt_digit_scan(CURRENT_LOCALE->cat[LC_TIME], &s);
		if (a >= 0) { *v = a; *ps = s; return 0; }
	}
	return number(ps, maxd, sign, v);
}

/* pin: the era whose own format this is, or -1 */
static const char *parse(const char *s, const char *f, struct tm *tm, int *have, int nocase, int depth, int pin)
{
	const struct __lcat *lt = CURRENT_LOCALE->cat[LC_TIME];
	int century = -1, yy = -1, pm = -1, h12 = -1, era = pin;
	long long v, ey = -1;
	struct __era e;
	*have = 0;

	for (; *f; f++) {
		int width = 0, mod = 0;
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
		if (*f == 'E' || *f == 'O') mod = *f++;
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
			s = parse(s, erafmt(mod, T_ERA_DTFMT, T_DTFMT, "%a %b %e %H:%M:%S %Y"), tm, &h, nocase, depth + 1, -1);
			if (!s) return 0;
			*have |= h;
			break;
		}
		case 'C':
			if (mod == 'E' && __era_count(lt)) {
				/* the name of an era (the longest that matches); once
				 * the era is known (an era year, or its own format),
				 * the name of that era, as glibc */
				unsigned i, n = __era_count(lt);
				size_t best = 0, l;
				int k = -1;
				if (era >= 0) {
					__era_get(lt, (unsigned)era, &e);
					l = strlen(e.name);
					if (!l || strncmp(s, e.name, l)) return 0;
					s += l; *have |= HAVE_YEAR;
					break;
				}
				for (i = 0; i < n; i++) {
					__era_get(lt, i, &e);
					l = strlen(e.name);
					if (l > best && !strncmp(s, e.name, l)) { best = l; k = (int)i; }
				}
				if (k >= 0) { era = k; s += best; *have |= HAVE_YEAR; break; }
			}
			if (altnumber(&s, width ? width : 2, 1, &v, mod)) return 0;
			century = (int)v; *have |= HAVE_YEAR;
			break;
		case 'd': case 'e':
			if (altnumber(&s, 2, 0, &v, mod) || v < 1 || v > 31) return 0;
			tm->tm_mday = (int)v; *have |= HAVE_MDAY;
			break;
		case 'D': {
			int h;
			if (!(s = parse(s, "%m/%d/%y", tm, &h, nocase, depth + 1, -1))) return 0;
			*have |= h;
			if ((h & (HAVE_YEAR | HAVE_FULLYEAR)) == HAVE_YEAR) { yy = (tm->tm_year + 1900) % 100; century = -1; }
			break;
		}
		case 'F': {
			const char *e = width ? s + width : 0, *b = s;
			if (number(&s, width ? width : 18, 1, &v) || *s != '-') return 0;
			if (v - 1900 < INT_MIN || v - 1900 > INT_MAX) return 0;
			tm->tm_year = (int)(v - 1900); century = yy = -1; s++;
			if (number(&s, 2, 0, &v) || v < 1 || v > 12 || *s != '-') return 0;
			tm->tm_mon = (int)v - 1; s++;
			if (altnumber(&s, 2, 0, &v, mod) || v < 1 || v > 31) return 0;
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
			if (altnumber(&s, 2, 0, &v, mod) || v > 23) return 0;
			tm->tm_hour = (int)v; h12 = -1; *have |= HAVE_HOUR;
			break;
		case 'I':
			if (altnumber(&s, 2, 0, &v, mod) || v < 1 || v > 12) return 0;
			h12 = (int)v % 12; *have |= HAVE_HOUR;
			break;
		case 'j':
			if (number(&s, 3, 0, &v) || v < 1 || v > 366) return 0;
			tm->tm_yday = (int)v - 1; *have |= HAVE_YDAY;
			break;
		case 'm':
			if (altnumber(&s, 2, 0, &v, mod) || v < 1 || v > 12) return 0;
			tm->tm_mon = (int)v - 1; *have |= HAVE_MON;
			break;
		case 'M':
			if (altnumber(&s, 2, 0, &v, mod) || v > 59) return 0;
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
			                  c == 'X' ? erafmt(mod, T_ERA_TFMT, T_TFMT, "%H:%M:%S") : "%H:%M:%S";
			if (depth > 3) return 0;
			if (!(s = parse(s, sub, tm, &h, nocase, depth + 1, -1))) return 0;
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
			if (altnumber(&s, 2, 0, &v, mod) || v > 60) return 0;
			tm->tm_sec = (int)v; *have |= HAVE_SEC;
			break;
		case 'u':
			if (number(&s, 1, 0, &v) || v < 1 || v > 7) return 0;
			tm->tm_wday = (int)v % 7; *have |= HAVE_WDAY;
			break;
		case 'U': case 'W': case 'V':
			if (altnumber(&s, 2, 0, &v, mod) || v > 53) return 0;
			break;
		case 'w':
			if (altnumber(&s, 1, 0, &v, mod) || v > 6) return 0;
			tm->tm_wday = (int)v; *have |= HAVE_WDAY;
			break;
		case 'x': {
			int h;
			if (depth > 3) return 0;
			if (!(s = parse(s, erafmt(mod, T_ERA_DFMT, T_DFMT, "%m/%d/%y"), tm, &h, nocase, depth + 1, -1))) return 0;
			*have |= h;
			if ((h & (HAVE_YEAR | HAVE_FULLYEAR)) == HAVE_YEAR) { yy = (tm->tm_year + 1900) % 100; century = -1; }
			break;
		}
		case 'y':
			if (mod == 'E' && __era_count(lt)) {
				/* the year within the era (as glibc: up to 4 digits);
				 * without its name, the first era it fits */
				unsigned i, n = __era_count(lt);
				const char *b = s;
				if (number(&s, 4, 0, &v)) return 0;
				if (era >= 0) {
					__era_get(lt, (unsigned)era, &e);
					if (!__era_year_ok(&e, v)) return 0;
				} else {
					for (i = 0; i < n; i++) {
						__era_get(lt, i, &e);
						if (__era_year_ok(&e, v)) break;
					}
					if (i < n) era = (int)i;
				}
				if (era >= 0) { ey = v; *have |= HAVE_YEAR; break; }
				s = b;   /* no era: an ordinary %y */
			}
			if (altnumber(&s, 2, 1, &v, mod) || v < 0 || v > 99) return 0;
			yy = (int)v; *have |= HAVE_YEAR;
			break;
		case 'Y':
			if (mod == 'E' && depth <= 3) {
				/* the full era year: the format of one of the eras */
				unsigned i, n = __era_count(lt);
				const char *r = 0;
				int h = 0;
				for (i = 0; i < n && !r; i++) {
					__era_get(lt, i, &e);
					if (*e.format) r = parse(s, e.format, tm, &h, nocase, depth + 1, (int)i);
				}
				if (r && (h & HAVE_YEAR)) {
					s = r; *have |= h; century = yy = -1; era = -1;
					break;
				}
			}
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
	if (era >= 0) {
		/* the era's first year, or the year counted from its offset */
		__era_get(lt, (unsigned)era, &e);
		tm->tm_year = (int)(e.start[0] + (ey >= 0 ? (ey - e.offset) * e.dir : 0));
		*have |= HAVE_YEAR | HAVE_FULLYEAR;
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
