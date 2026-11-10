/* strtof, strtod, strtold and sscanf against glibc (both correctly
 * rounded): random digit strings, values near halfway points, hex
 * floats, special values, and endptr positions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "out.h"

static void bits(const void *p, size_t n)
{
	const unsigned char *b = p;
	while (n--) ox(b[n] | 0x100);
}

static void test(const char *s)
{
	char *e;
	float f; double d; long double l;
	os(s); oc(' ');
	errno = 0; f = strtof(s, &e); bits(&f, 4); oc('/'); oi(e - s); oc('/'); oi(errno); oc(' ');
	errno = 0; d = strtod(s, &e); bits(&d, 8); oc('/'); oi(e - s); oc('/'); oi(errno); oc(' ');
	errno = 0; l = strtold(s, &e); bits(&l, 10); oc('/'); oi(e - s); oc('/'); oi(errno);
	onl();
}

static char sbuf[30000];

int main(void)
{
	static const char *fixed[] = {
		"0", "-0", "+0.0", "1", "1.5", "-2.25e3", ".5", "5.", ".", "-.", "e5", "1e", "1e+", "1e-x", "1ee5", "  \t 42",
		"inf", "-INF", "infinity", "INFINITYx", "infin", "nan", "-nan", "NaN(123abc)", "nan(", "nan(x y)", "in", "na",
		"0x", "0x1p", "0x1p-", "0xp1", "0x.8", "0x1.8p1", "-0X1.FFFFFFFFFFFFFp1023", "0x1p-1074", "0x1p-1075", "0x1.1p-1075",
		"0x1p-149", "0x1.000002p0", "0x1.000001p0", "0x1.0000010000000000001p0", "0x.000000000000000000000000001p0",
		"1e309", "1e-400", "1e4933", "1e-4951", "1e-4952", "2.4703282292062327e-324", "2.4703282292062328e-324",
		"4.9406564584124654e-324", "2.2250738585072011e-308", "1.7976931348623158e308",
		"1.7976931348623157e308", "1.797693134862315807e308", "3.4028235677973366e38", "3.4028236e38",
		"1.4012984643e-45", "7.0064923216e-46", "7.0064923217e-46", "0.1", "0.2", "0.3", "123456789012345678901234567890",
		"9007199254740993", "9007199254740992.5", "9007199254740993.0000000000000000000001", "18446744073709551616",
		"1.18973149535723176502e+4932", "1.18973149535723176503e+4932", "3.64519953188247460253e-4951",
		"000000000000000000000000000000001", "0.000000000000000000000000000000001e33", "1e-99999999999", "1e99999999999",
		"123.456e-2xyz", "0e0", "0x0p0", "-0x0", "08", "0.0000",
	};
	/* Not compared: 2.2250738585072012e-308 and 1.17549435e-38, which round
	 * up to the smallest normal number: whether that is an underflow
	 * (ERANGE) is implementation-defined, and glibc answers differently
	 * for double and float.  This library detects tininess after rounding. */
	size_t i;
	for (i = 0; i < sizeof fixed / sizeof *fixed; i++) test(fixed[i]);

	/* random decimal strings */
	for (i = 0; i < 20000; i++) {
		char *p = sbuf;
		int nd = 1 + (int)(rng() % (i % 10 == 0 ? 120 : 20)), k, pt = (int)(rng() % (nd + 1));
		if (rng() % 2) *p++ = '-';
		for (k = 0; k < nd; k++) {
			if (k == pt) *p++ = '.';
			*p++ = (char)('0' + rng() % 10);
		}
		if (rng() % 2) {
			int ex = (int)(rng() % 700) - 350;
			if (i % 5 == 0) ex = (int)(rng() % 9900) - 4950;
			p += snprintf(p, 20, "e%d", ex);
		}
		*p = 0;
		test(sbuf);
	}
	/* exact halfway points of doubles and floats, and their neighbours */
	for (i = 0; i < 4000; i++) {
		union { double d; uint64_t u; } a, b;
		long double mid;
		int len;
		a.u = rng() & 0x7fefffffffffffffULL;
		if (i % 4 == 0) a.u &= 0x000fffffffffffffULL;   /* subnormals */
		b.u = a.u + 1;
		mid = ((long double)a.d + (long double)b.d) / 2;   /* exact in long double */
		len = snprintf(sbuf, sizeof sbuf, "%.800Le", mid);
		(void)len;
		test(sbuf);
		/* the same with the last nonzero digit lowered and raised */
		{
			char *q = strchr(sbuf, 'e');
			char *z = q - 1;
			while (*z == '0') z--;
			if (*z > '0' && *z != '.') { (*z)--; test(sbuf); (*z) += 2; if (*z <= '9') test(sbuf); }
		}
	}
	/* hexadecimal floats */
	for (i = 0; i < 5000; i++) {
		unsigned long long r1 = rng(), r2 = rng(), r3 = rng(), r4 = rng(), r5 = rng();
		snprintf(sbuf, sizeof sbuf, "%s0x%llx.%llxp%d", r1 % 2 ? "-" : "", r2 >> (r3 % 64), r4, (int)(r5 % 34000) - 17000);
		test(sbuf);
	}
	/* sscanf */
	{
		static const char *sc[] = { "12 abc 3.5", "  -7xyz", "0x1f 017 9", "1e5 2.5e-3 .5", "abc", "", "1,2,3",
			"[hello world]", "12345678901234567890", /* not "  " with %3c: glibc stores 2 characters, C says input failure;
			 * not "3.0e+ 7": glibc accepts "3.0e+" for %e, C says it is a matching failure */ "nan inf -INFINITY", "a bc", "42 %", "x=5;y=6" };
		for (i = 0; i < sizeof sc / sizeof *sc; i++) {
			int a1 = -1, a2 = -1, n1 = -1, n2 = -1, r;
			unsigned u1 = 0; long long ll = 0; double d1 = 0, d2 = 0; float f1 = 0; long double L = 0;
			char s1[64] = "", s2[64] = "", c1[4] = "";
			os(sc[i]); os(" | ");
			r = sscanf(sc[i], "%d %s %lf", &a1, s1, &d1); oi(r); oc(','); oi(a1); oc(','); os(s1); oc(','); bits(&d1, 8); oc(' ');
			r = sscanf(sc[i], "%i %i %i", &a1, &a2, &n1); oi(r); oc(','); oi(a1); oc(','); oi(a2); oc(','); oi(n1); oc(' ');
			r = sscanf(sc[i], "%x %o %u", &u1, (unsigned *)&a2, (unsigned *)&n1); oi(r); oc(','); oi(u1); oc(' ');
			r = sscanf(sc[i], "%e %lg %Lf", &f1, &d2, &L); oi(r); oc(','); bits(&f1, 4); oc(','); bits(&d2, 8); oc(','); bits(&L, 10); oc(' ');
			r = sscanf(sc[i], "%3c%n %[a-z ]%n", c1, &n1, s2, &n2); oi(r); oc(','); oi(n1); oc(','); oi(n2); oc(','); os(s2); oc(' ');
			r = sscanf(sc[i], "%*d %lld", &ll); oi(r); oc(','); oi(ll); oc(' ');
			r = sscanf(sc[i], "[%[^]]]", s2); oi(r); oc(','); os(s2); oc(' ');
			r = sscanf(sc[i], "x=%d;y=%d", &a1, &a2); oi(r); oc(','); oi(a1); oc(','); oi(a2); oc(' ');
			r = sscanf(sc[i], "%2d%3s", &a1, s1); oi(r); oc(','); oi(a1); oc(','); os(s1); oc(' ');
			r = sscanf(sc[i], "%d %%", &a1); oi(r);
			onl();
		}
		{
			char *ms = 0; int r, a = 0, b = 0;
			r = sscanf("dynamic string", "%ms", &ms); oi(r); os(ms ? ms : "null"); free(ms); onl();
			r = sscanf("5 6", "%2$d %1$d", &a, &b); oi(r); oi(a); oi(b); onl();
		}
	}
	oflush();
	return 0;
}
