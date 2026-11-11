#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include "out.h"

#ifdef __GLIBC__
/* qsort_r() is POSIX.1-2024; glibc declares it only for _GNU_SOURCE, which
 * would also turn on C23's "0b101" in strtol() */
void qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *);
#endif

static const char *inputs[] = {
	"0", "-0", "+0", "1", "-1", "  42", "\t\n\v\f\r 42x", "0x", "0x1g", "0X1F", "-0x10", "010", "08", "0b101",
	"2147483647", "2147483648", "-2147483648", "-2147483649",
	"9223372036854775807", "9223372036854775808", "-9223372036854775808", "-9223372036854775809",
	"18446744073709551615", "18446744073709551616", "-18446744073709551615", "-18446744073709551616",
	"99999999999999999999999999", "-99999999999999999999999", "zz", "ZZZZZZZZZZZZZZZ", "+", "-", "", "  ", "+-1",
	"1e5", "123abc", "0777777777777777777777", "01777777777777777777777", "0x8000000000000000",
	"-0x8000000000000000", "-0x8000000000000001", "0xffffffffffffffff", "0x10000000000000000",
};
static const int bases[] = { 0, 2, 8, 10, 16, 36 };

static int icmp(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }
static int icmp_r(const void *a, const void *b, void *arg) { (*(int *)arg)++; return icmp(a, b); }

int main(void)
{
	size_t i, j;
	for (i = 0; i < sizeof inputs / sizeof *inputs; i++)
		for (j = 0; j < sizeof bases / sizeof *bases; j++) {
			const char *s = inputs[i]; char *e; int b = bases[j];
			os(s); oc('/'); oi(b); oc(':');
			errno = 0; oi(strtol(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			errno = 0; oi(strtoll(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			errno = 0; ou(strtoul(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			errno = 0; ou(strtoull(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			errno = 0; oi(strtoimax(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			errno = 0; ou(strtoumax(s, &e, b)); oc(','); oi(e - s); oc(','); oi(errno); oc(' ');
			if (b == 10) { oi(atoi(s)); oc(' '); oi(atol(s)); oc(' '); oi(atoll(s)); }
			onl();
		}
	/* random round trips */
	for (i = 0; i < 2000; i++) {
		char buf[80], *p = buf + sizeof buf; unsigned long long v = rng(), sh = rng() % 64; /* sequenced: operand order is unspecified */
		v >>= sh;
		int b = 2 + (int)(rng() % 35);
		*--p = 0;
		do { int d = (int)(v % (unsigned)b); *--p = (char)(d < 10 ? '0' + d : 'a' + d - 10); } while (v /= (unsigned)b);
		ou(strtoull(p, 0, b)); oc(' '); oi(strtol(p, 0, b)); oc(' ');
	}
	onl();
	for (i = 0; i < 200; i++) {
		int n = (int)(rng() % 300), a[300], k, cnt = 0;
		for (k = 0; k < n; k++) { unsigned long long r = rng(), m = 1 + rng() % 1000; a[k] = (int)(r % m) - 50; }
		if (i % 2) qsort(a, (size_t)n, sizeof *a, icmp); else qsort_r(a, (size_t)n, sizeof *a, icmp_r, &cnt);
		for (k = 0; k < n; k++) { oi(a[k]); oc(' '); }
		for (k = -60; k < 60; k += 7) { int *f = bsearch(&k, a, (size_t)n, sizeof *a, icmp); oc(f ? 'y' : 'n'); if (f) oi(*f); }
		onl();
	}
	/* sorted, reverse, all-equal (worst-case shapes) */
	{ static int a[100000]; int k;
	  for (k = 0; k < 100000; k++) a[k] = 100000 - k; qsort(a, 100000, sizeof *a, icmp);
	  for (k = 1; k < 100000; k++) if (a[k-1] > a[k]) os("unsorted");
	  for (k = 0; k < 100000; k++) a[k] = k % 3; qsort(a, 100000, sizeof *a, icmp);
	  for (k = 1; k < 100000; k++) if (a[k-1] > a[k]) os("unsorted"); }
	{ div_t d = div(-7, 2); ldiv_t l = ldiv(7, -2); lldiv_t ll = lldiv(-7, -2); imaxdiv_t im = imaxdiv(INTMAX_MIN, 3);
	  oi(d.quot); oi(d.rem); oi(l.quot); oi(l.rem); oi(ll.quot); oi(ll.rem); oi(im.quot); oi(im.rem);
	  oi(abs(-5)); oi(labs(LONG_MIN + 1)); oi(llabs(-3)); oi(imaxabs(-9)); onl(); }
	oflush();
	return 0;
}
