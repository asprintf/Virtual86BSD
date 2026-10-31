/* strtol family.  One core routine converts to unsigned long long with
 * an explicit range; the wrappers apply signedness and limits. */
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

static unsigned long long conv(const char *s, char **end, int base,
                               unsigned long long lim)
{
	const char *p = s, *start;
	int neg = 0, any = 0, over = 0;
	unsigned long long acc = 0, cutoff, v;
	unsigned cutlim;

	if (base < 0 || base == 1 || base > 36) {
		if (end) *end = (char *)s;
		errno = EINVAL;
		return 0;
	}
	while (isspace((unsigned char)*p)) p++;
	if (*p == '+' || *p == '-') neg = *p++ == '-';
	start = p;
	if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x' &&
	    isxdigit((unsigned char)p[2])) {
		p += 2; base = 16;
	} else if (base == 0) {
		base = *p == '0' ? 8 : 10;
	}
	/* Overflow is detected against lim for positive and lim+1 for negative
	 * results of signed types; the caller passes lim accordingly. */
	cutoff = lim / (unsigned)base;
	cutlim = (unsigned)(lim % (unsigned)base);
	for (;; p++) {
		unsigned c = (unsigned char)*p;
		if (c - '0' < 10) v = c - '0';
		else if ((c | 32) - 'a' < 26) v = (c | 32) - 'a' + 10;
		else break;
		if (v >= (unsigned)base) break;
		any = 1;
		if (over || acc > cutoff || (acc == cutoff && v > cutlim)) over = 1;
		else acc = acc * (unsigned)base + v;
	}
	if (end) *end = (char *)(any ? p : s);
	(void)start;
	/* unsigned types (lim == max) saturate to max even for '-' input */
	if (over) { errno = ERANGE; return neg && lim != ULLONG_MAX ? -lim : lim; }
	return neg ? -acc : acc;
}

/* Signed conversion: magnitude limit max for '+', max+1 for '-'. */
static long long sconv(const char *s, char **end, int base, long long min, long long max)
{
	const char *p = s;
	while (isspace((unsigned char)*p)) p++;
	if (*p == '-') {
		(void)min;
		return (long long)conv(s, end, base, (unsigned long long)max + 1);
	}
	return (long long)conv(s, end, base, (unsigned long long)max);
}

long strtol(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return (long)sconv(s, e, b, LONG_MIN, LONG_MAX); }
long long strtoll(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return sconv(s, e, b, LLONG_MIN, LLONG_MAX); }
intmax_t strtoimax(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return sconv(s, e, b, INTMAX_MIN, INTMAX_MAX); }
unsigned long strtoul(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return conv(s, e, b, ULONG_MAX); }
unsigned long long strtoull(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return conv(s, e, b, ULLONG_MAX); }
uintmax_t strtoumax(const char *__RESTRICT s, char **__RESTRICT e, int b)
{ return conv(s, e, b, UINTMAX_MAX); }

int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
long long atoll(const char *s) { return strtoll(s, 0, 10); }
