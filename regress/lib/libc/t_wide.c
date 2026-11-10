/* t_wide.c - the wide formatted functions (differential): swprintf and
 * swscanf, fwprintf and fwscanf on a file, in C, C.UTF-8 and the test
 * locales.  Every wide character passes swprintf; widths, precisions,
 * %c, %[ and %n count characters. */
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "out.h"

static void ows(const wchar_t *w, int n)
{
	int i;
	for (i = 0; i < n; i++) { ox((unsigned long long)(unsigned)w[i]); oc('.'); }
}

static void pr(const wchar_t *fmt, ...)
{
	wchar_t b[128];
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = vswprintf(b, 128, fmt, ap);
	va_end(ap);
	oi(r); oc(' ');
	if (r > 0) ows(b, r);
	onl();
}

static void one(void)
{
	wchar_t a[16], b[16];
	char c[32], *m;
	wchar_t *wm;
	int r, n, n2, d;
	double y;

	pr(L"x\x20ac%ls|%lc|%5ls|%-4.1ls|", L"\xe9\x4e2d", 0x263a, L"\x20ac", L"\x5c\x7e");
	pr(L"%s|%c|%.1f|%'d|%5.2s|", "ab\\~", 'z', 1.5, 1234567, "xyz");
	pr(L"%3$ls %1$d %2$lc", 7, 0x10348, L"\x1f600");
	pr(L"%ls", L"\xd800");
	pr(L"[%*ls] [%-*lc]", 4, L"\x3042", 3, 0xff8a);
	/* too small: -1 */
	{
		wchar_t s4[4];
		oi(swprintf(s4, 4, L"%ls", L"abcd")); onl();
	}

	memset(a, 0, sizeof a); memset(b, 0, sizeof b); memset(c, 0, sizeof c);
	n = n2 = -1;
	r = swscanf(L"\x65e5\x672c\x8a9e-x 12", L"%l[\x65e5\x672c]%n%2lc%n%d", a, &n, b, &n2, &d);
	oi(r); oc(' '); ows(a, 2); ows(b, 2); oi(n); oc(' '); oi(n2); onl();
	r = swscanf(L"\x20ac\x4e2d 5 xy \xe9", L"%ls %d %s %lc", a, &d, c, b);
	oi(r); oc(' '); ows(a, 2); oi(d); oc(' '); os(c); oc(' '); ows(b, 1); onl();
	r = swscanf(L"\x20ac=7.25e1z", L"\x20ac=%lf%n%c", &y, &n, c);
	oi(r); oc(' '); oi((long long)(y * 100)); oc(' '); oi(n); oc(' '); oi(c[0]); onl();
	r = swscanf(L" 0x1fq -12", L"%x%lc%d", &d, a, &n);
	oi(r); oc(' '); oi(d); oc(' '); ows(a, 1); oi(n); onl();
	r = swscanf(L"ab\x3042\x3044 cd", L"%3c%[^ ]", c, c + 16);
	oi(r); oc(' '); os(r > 0 ? c + 16 : ""); onl();
	r = swscanf(L"\x4e2d\x6587 rest", L"%ms %mls", &m, &wm);
	oi(r);
	if (r == 2) { oc(' '); os(m); oc(' '); ows(wm, (int)wcslen(wm)); free(m); free(wm); }
	onl();
	oi(swscanf(L"", L"%d", &d)); oc(' ');
	oi(swscanf(L"  ", L" %lc", a)); oc(' ');
	oi(swscanf(L"x", L"%d", &d)); onl();

	/* a file */
	{
		FILE *f = tmpfile();
		if (f) {
			wint_t k;
			oi(fwprintf(f, L"%ls %d %lc\n", L"abc", 42, L'q')); oc(' ');
			rewind(f);
			memset(a, 0, sizeof a);
			r = fwscanf(f, L"%ls%d %lc%n", a, &d, b, &n);
			oi(r); oc(' '); ows(a, 3); oi(d); oc(' '); oi(n); oc(' ');
			k = fgetwc(f);
			ox(k); onl();
			fclose(f);
		}
	}
}

int main(void)
{
	const char *tl = getenv("TEST_LOCALES");
	char list[2048], *p;
	os("== C"); onl();
	one();
	if (setlocale(LC_ALL, "C.UTF-8")) { os("== C.UTF-8"); onl(); one(); }
	if (tl && strlen(tl) < sizeof list) {
		strcpy(list, tl);
		for (p = strtok(list, " "); p; p = strtok(0, " ")) {
			if (!setlocale(LC_ALL, p)) continue;
			os("== "); os(p); onl();
			one();
		}
	}
	oflush();
	return 0;
}
