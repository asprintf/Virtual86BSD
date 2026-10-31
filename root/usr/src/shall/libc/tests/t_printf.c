/* snprintf against glibc: integers, strings, and floating point with
 * random bit patterns (normal, subnormal, inf, nan) in every format. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "out.h"

static char buf[20000];

static double rdouble(void)
{
	union { double d; uint64_t u; } v;
	/* every rng() call in its own statement: the order of evaluation of
	 * operands is unspecified, and gcc and c17 differ */
	uint64_t r1, r2, r3;
	switch (rng() % 6) {
	case 0: v.u = rng(); break;                                   /* anything */
	case 1: v.u = rng() & 0x800fffffffffffffULL; break;            /* subnormal */
	case 2: v.d = (double)(int64_t)(rng() % 2000000) / 64; break;  /* exact binary fractions */
	case 3: r1 = rng(); r2 = rng(); v.d = (double)(int64_t)(r1 % 100000) * 0.5 + 0.5 * (double)(r2 % 3); break;  /* ties */
	case 4: r1 = rng(); r2 = rng(); r3 = rng();
		v.u = (r1 & 0x8000000000000000ULL) | (uint64_t)(1023 + r2 % 80 - 40) << 52 | (r3 & 0xfffffffffffffULL); break;
	default: r1 = rng(); r2 = rng(); v.d = (double)(int64_t)r1 / (double)(1 + r2 % 1000000); break;
	}
	return v.d;
}

static long double rldouble(void)
{
	union { long double f; struct { uint64_t m; uint16_t se; } i; } v;
	memset(&v, 0, sizeof v);
	switch (rng() % 4) {
	case 0: v.i.m = rng() | 1ULL << 63; v.i.se = (uint16_t)rng() & 0xfffe; if (!(v.i.se & 0x7fff)) v.i.m >>= 1; break;
	case 1: v.i.m = rng() >> 1; v.i.se = (uint16_t)(rng() & 0x8000); break;   /* subnormal */
	case 2: v.i.m = rng() | 1ULL << 63; v.i.se = (uint16_t)(16383 + rng() % 200 - 100); break;
	default: v.f = (long double)rdouble(); break;
	}
	return v.f;
}

int main(void)
{
	static const char *ff[] = { "%f", "%e", "%g", "%a", "%.0f", "%.0e", "%.0g", "%.1f", "%.3e", "%.10g",
		"%.17g", "%.20e", "%#.0f", "%#g", "%#.3g", "%+.4f", "% .2e", "%012.3f", "%-12.4g|", "%.50f",
		"%.0a", "%.3a", "%#a", "%A", "%E", "%G", "%F", "%.1a", "%.13a", "%.30e", "%.700f", "%5.1g", "%.99g" };
	static const char *lf[] = { "%Lf", "%Le", "%Lg", "%.0Lf", "%.25Le", "%.30Lg", "%#.0Le", "%.5Lf", "%.40Lg", "%.21Lg",
		"%La", "%.3La", "%.0La", "%LA", "%#.1La", "%.16La" };
	static const char *intf[] = { "%d", "%5d", "%-5d|", "%05d", "%+d", "% d", "%.3d", "%.0d", "%x", "%#x", "%#o",
		"%#.3o", "%o", "%X", "%#X", "%u", "%10.4u", "%-#10x|", "%hhd", "%hd", "%hhu", "%.0x", "%#.0o" };
	long i;
	size_t j;
	int n;

	for (i = 0; i < 6000; i++) {
		double d = rdouble();
		for (j = 0; j < sizeof ff / sizeof *ff; j++) {
			n = snprintf(buf, sizeof buf, ff[j], d);
			oi(n); oc(' '); os(buf); oc('\n');
		}
	}
	for (i = 0; i < 3000; i++) {
		long double x = rldouble();
		for (j = 0; j < sizeof lf / sizeof *lf; j++) {
			n = snprintf(buf, sizeof buf, lf[j], x);
			oi(n); oc(' '); os(buf); oc('\n');
		}
	}
	{
		static const double sp[] = { 0.0, -0.0, 1.0 / 0.0, -1.0 / 0.0, 0.5, 1.5, 2.5, 0.125, 1e300, 1e-300,
			4.9406564584124654e-324, 2.2250738585072014e-308, 1.7976931348623157e308, 123456789012345678.0, 0.1, 0.3 };
		for (j = 0; j < sizeof sp / sizeof *sp; j++) {
			size_t k;
			for (k = 0; k < sizeof ff / sizeof *ff; k++) {
				n = snprintf(buf, sizeof buf, ff[k], sp[j]);
				oi(n); oc(' '); os(buf); oc('\n');
			}
		}
	}
	for (i = 0; i < 20000; i++) {
		uint64_t r1 = rng(), r2 = rng(), w1 = rng(), w2 = rng(), w3 = rng();
		long long v = (long long)(r1 >> (r2 % 64));
		if (rng() % 2) v = -v;
		for (j = 0; j < sizeof intf / sizeof *intf; j++) {
			n = snprintf(buf, sizeof buf, intf[j], (int)v);
			oi(n); oc(' '); os(buf); oc('\n');
		}
		n = snprintf(buf, sizeof buf, "%lld %llx %llo %ld %lu %zu %zd %jd %ju %td %*d %-*d| %.*d", v, (unsigned long long)v,
		             (unsigned long long)v, (long)v, (unsigned long)v, (size_t)v, (ssize_t)v, (intmax_t)v, (uintmax_t)v,
		             (ptrdiff_t)v, (int)(w1 % 30), (int)v, (int)(w2 % 30) - 15, (int)v, (int)(w3 % 25) - 5, (int)v);
		oi(n); oc(' '); os(buf); oc('\n');
	}
	/* strings, chars, positional arguments, %n, truncation */
	{
		int k1 = 0, k2 = 0;
		n = snprintf(buf, sizeof buf, "[%s|%10s|%-10s|%.2s|%c|%5c|%-3c|%%|%s]", "hello", "hi", "left", "xyz", 'A', 'B', 'C', "");
		oi(n); os(buf); onl();
		n = snprintf(buf, sizeof buf, "%2$s %1$s %3$d %3$x %4$*5$d %6$.*7$f", "world", "hello", 255, 42, 6, 3.14159, 2);
		oi(n); os(buf); onl();
		n = snprintf(buf, sizeof buf, "abc%ndef%hhn", &k1, (signed char *)&k2);
		oi(n); os(buf); oi(k1); oi(k2 & 0xff); onl();
		n = snprintf(buf, 5, "%s", "truncated output"); oi(n); os(buf); onl();
		n = snprintf(0, 0, "%d-%s", 12345, "abc"); oi(n); onl();
		n = snprintf(buf, sizeof buf, "%p %p", (void *)0x1234, (void *)0); oi(n); os(buf); onl();
		n = snprintf(buf, sizeof buf, "%lc%ls|%5ls|%.2ls", L'x', L"wide", L"ab", L"abcdef"); oi(n); os(buf); onl();
		n = snprintf(buf, sizeof buf, "%d %s", INT_MIN, (char *)0); oi(n); os(buf); onl();
		n = snprintf(buf, sizeof buf, "%.0f %.0f %.0f %.0f %.1f %.2f", 0.5, 1.5, 2.5, -0.5, 0.25, 0.125);
		oi(n); os(buf); onl();
	}
	oflush();
	return 0;
}
