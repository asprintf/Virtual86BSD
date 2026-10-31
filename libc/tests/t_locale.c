/* Differential: locales (the built-in C and C.UTF-8, and the locales of
 * TEST_LOCALES, in LOCPATH, made by run.sh with localedef), langinfo,
 * localeconv, wide character classes and mappings, widths, conversions,
 * locale-dependent printf/strtod/strftime/strptime/strfmon, wide
 * strings, swprintf, regex and fnmatch in UTF-8, iconv.
 * Excluded (deliberate differences, see STATUS.md): bytes 0x80-0xff of
 * the POSIX locale, code points above 0x10FFFF in UTF-8, the "combining"
 * classes of glibc's C.utf8, strfmon "%+n" in the POSIX locale. */
#define _GNU_SOURCE
#include <locale.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <monetary.h>
#include <regex.h>
#include <fnmatch.h>
#include <iconv.h>
#include "out.h"

#ifndef ABALTMON_1
#define ABALTMON_1 _NL_ABALTMON_1
#endif

static unsigned long long h;
static void H(unsigned long long v) { h = h * 1000003 + v; }

static void one(const char *name)
{
	char *r = setlocale(LC_ALL, name), buf[512];
	unsigned c, k;
	struct lconv *lc;
	int utf8, posix = !strcmp(name, "C") || !strcmp(name, "POSIX");
	os("== "); os(name); os(": "); os(r ? r : "(null)"); onl();
	if (!r) return;
	utf8 = MB_CUR_MAX > 1;
	oi((long long)MB_CUR_MAX); onl();
	{
		static const nl_item it[] = { CODESET, RADIXCHAR, THOUSEP, YESEXPR, NOEXPR, CRNCYSTR, D_T_FMT, D_FMT, T_FMT,
			T_FMT_AMPM, AM_STR, PM_STR, DAY_1, DAY_7, ABDAY_2, MON_1, MON_12, ABMON_5, ALTMON_3, ABALTMON_1 + 2 };
		for (k = 0; k < sizeof it / sizeof *it; k++) { os(nl_langinfo(it[k])); oc('|'); }
		onl();
	}
	lc = localeconv();
	os(lc->decimal_point); oc('|'); os(lc->thousands_sep); oc('|'); oi(lc->grouping[0]); oc('|');
	os(lc->int_curr_symbol); oc('|'); os(lc->currency_symbol); oc('|'); os(lc->mon_decimal_point); oc('|');
	os(lc->mon_thousands_sep); oc('|'); os(lc->positive_sign); oc('|'); os(lc->negative_sign); oc('|');
	oi(lc->frac_digits); oc(' '); oi(lc->p_cs_precedes); oc(' '); oi(lc->n_sign_posn); oc(' '); oi(lc->int_frac_digits); onl();

	/* classes, maps, widths over all code points */
	for (k = 0; k < 12; k++) {
		static const char *n[] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct",
			"space", "upper", "xdigit" };
		wctype_t t = wctype(n[k]);
		h = 0;
		for (c = 0; c < 0x110000; c++) H(iswctype(c, t) != 0);
		os(n[k]); oc(' '); ox(h); onl();
	}
	h = 0; for (c = 0; c < 0x110000; c++) H(towupper(c)); os("toupper "); ox(h); onl();
	h = 0; for (c = 0; c < 0x110000; c++) H(towlower(c)); os("tolower "); ox(h); onl();
	h = 0; for (c = 0; c < 0x110000; c++) H(towctrans(c, wctrans("toupper"))); os("towctrans "); ox(h); onl();
	h = 0; for (c = 0; c < 0x110000; c++) H((unsigned)wcwidth(c)); os("wcwidth "); ox(h); onl();
	h = 0;
	for (c = 0; c < 256; c++) {
		H(!!isalpha(c)); H(!!isupper(c)); H(!!islower(c)); H(!!ispunct(c)); H(!!isspace(c)); H(!!isprint(c));
		H(!!isalnum(c)); H(!!iscntrl(c)); H(!!isgraph(c)); H((unsigned)toupper(c)); H((unsigned)tolower(c));
		if (c < 0x80 || !posix) H(btowc(c));
	}
	os("bytes "); ox(h); onl();
	/* conversions of every character there is */
	h = 0;
	for (c = 1; c < 0x110000; c++) {
		char b[8];
		mbstate_t st;
		size_t l;
		wchar_t w;
		if (posix && c >= 0x80) break;
		memset(&st, 0, sizeof st);
		l = wcrtomb(b, (wchar_t)c, &st);
		if (l == (size_t)-1 || !l) continue;   /* (glibc gives 0 for some tag characters) */
		H(c); H(l); H((unsigned char)b[0]); H((unsigned char)b[l - 1]);
		memset(&st, 0, sizeof st);
		H(mbrtowc(&w, b, l, &st)); H((unsigned)w); H((unsigned)wctob(c));
		if (l > 1) { memset(&st, 0, sizeof st); H(mbrtowc(&w, b, l - 1, &st)); H(mbsinit(&st)); }
	}
	os("conv "); ox(h); onl();
	{
		static const char *const s[] = { "\xc3\xa9", "\xe0\x80\x80", "\xed\xa0\x80", "\xc3", "\xe2\x82\xac", "\xf0\x9f\x98\x80",
			"\x80", "\xe9t\xe9", "abc" };
		for (k = 0; k < sizeof s / sizeof *s; k++) {
			wchar_t w[16];
			mbstate_t st;
			size_t l;
			if (posix && (unsigned char)s[k][0] >= 0x80) continue;
			memset(&st, 0, sizeof st);
			l = mbrtowc(w, s[k], strlen(s[k]), &st);
			oi((long long)l); oc(' '); ox((unsigned)w[0]); oc(' ');
			oi((long long)mbstowcs(w, s[k], 16)); oc(' '); oi(mblen(s[k], strlen(s[k]))); onl();
		}
	}
	/* numbers and times in the locale */
	snprintf(buf, sizeof buf, "%'d|%'.2f|%f|%.3e|%'15.1f|%a|%g|%'i|%'u", 1234567, 9876543.215, 3.5, 12345.678, -1234567.25, 1.5,
		0.0001234, -1234, 999u);
	os(buf); onl();
	{
		char *e;
		static const char s1[] = "1,5", s2[] = "2.25";
		double d = strtod(s1, &e);
		oi((long long)(d * 10)); oc(' '); oi((long long)(e - s1));
		d = strtod(s2, &e); oc(' '); oi((long long)(d * 100)); oc(' '); oi((long long)(e - s2)); onl();
	}
	{
		struct tm tm = { .tm_year = 124, .tm_mon = 2, .tm_mday = 5, .tm_hour = 13, .tm_min = 7, .tm_sec = 9, .tm_wday = 2, .tm_yday = 64 };
		struct tm t2;
		char *e;
		strftime(buf, sizeof buf, "%a %A %b %B %c|%x|%X|%r|%p|%Ob|%OB|%Ex|%EX", &tm);
		os(buf); onl();
		for (k = 0; k < 12; k++) {
			tm.tm_mon = (int)k;
			strftime(buf, sizeof buf, "%B %d %Y %b", &tm);
			memset(&t2, 0, sizeof t2);
			e = strptime(buf, "%B %d %Y %b", &t2);
			oi(e ? *e : -1); oc(' '); oi(t2.tm_mon); oc(' ');
		}
		strftime(buf, sizeof buf, "%c", &tm);
		memset(&t2, 0, sizeof t2);
		e = strptime(buf, "%c", &t2);
		oi(e ? *e : -1); oc(' '); oi(t2.tm_hour); oc(' '); oi(t2.tm_wday); onl();
	}
	{
		/* eras and alternative digits: strftime, then strptime of its
		 * output (E and O modifiers) */
		static const int dates[][3] = { { 1868, 10, 23 }, { 1912, 7, 30 }, { 1926, 12, 25 }, { 1927, 1, 1 },
			{ 1989, 1, 7 }, { 1989, 1, 8 }, { 1990, 6, 15 }, { 2019, 4, 30 }, { 2019, 5, 1 },
			{ 2020, 1, 1 }, { 2024, 3, 5 }, { 1999, 12, 31 } };
		static const char *fm[] = { "%EY", "%EC%Ey", "%Ey %EC", "%EC", "%Ey", "%Ec", "%Ex", "%EX",
			"%Od %Om %OH %OM %OS", "%Oy", "%Oe %OI %Ow" };
		/* compared only where the locale has eras (the first 8 formats)
		 * or alternative digits (the next 3): without them glibc fails
		 * where POSIX falls back to the plain conversion (s_time.c) */
		int hasera = *nl_langinfo(ERA) != 0, hasalt = *nl_langinfo(ALT_DIGITS) != 0;
		unsigned j;
		for (k = 0; k < sizeof dates / sizeof *dates; k++)
			for (j = 0; j < sizeof fm / sizeof *fm; j++) {
				if (j >= 8 ? !hasalt : !hasera) continue;
				struct tm tm = { .tm_year = dates[k][0] - 1900, .tm_mon = dates[k][1] - 1, .tm_mday = dates[k][2],
				                 .tm_hour = 21, .tm_min = 7, .tm_sec = 9 }, t2;
				char *e;
				mktime(&tm);
				strftime(buf, sizeof buf, fm[j], &tm);
				memset(&t2, 0, sizeof t2);
				e = strptime(buf, fm[j], &t2);
				os(fm[j]); oc('='); os(buf); oc(' ');
				oi(e ? e - buf : -1); oc(' ');
				if (!e) { onl(); continue; }   /* the fields are then unspecified */
				oi(t2.tm_year); oc(' '); oi(t2.tm_mon); oc(' '); oi(t2.tm_mday); oc(' ');
				oi(t2.tm_hour); oc(' '); oi(t2.tm_min); oc(' '); oi(t2.tm_sec); onl();
			}
	}
	{
		static const char *f[] = { "%n", "%i", "%(n", "%!n", "%^n", "%12n", "%-12n|", "%#5n", "%=*#5n", "%.0n", "%.3i", "%(#6.2n" };
		static const double v[] = { 1234567.891, -1234.5, 0 };
		for (k = 0; k < sizeof f / sizeof *f; k++) {
			unsigned j;
			for (j = 0; j < 3; j++) {
				ssize_t rr = strfmon(buf, sizeof buf, f[k], v[j]);
				oi(rr); oc('['); os(rr < 0 ? "" : buf); os("] ");
			}
			onl();
		}
	}
	/* wide strings */
	{
		wchar_t w[200], *p;
		long v;
		int rr = swprintf(w, 200, L"[%5ls|%-6s|%.2ls|%lc|%d|%.3f|%5.1s|%'d]", L"h\x00e9\x00e9", "a", L"\x00e4\x00f6\x00fc", (wint_t)0x20ac,
			-42, 3.14159, "x", 1234567);
		if (rr >= 0 && wcstombs(buf, w, sizeof buf) != (size_t)-1) { oi(rr); oc(' '); os(buf); }
		else os("(unconvertible)");
		onl();
		v = wcstol(L"  -123abc", &p, 10);
		oi(v); oc(' '); oi(*p); oc(' '); oi(sgn(wcscasecmp(L"ABC\x00c4", L"abc\x00e4"))); oi(sgn(wcscasecmp(L"ab", L"abc"))); onl();
	}
	/* regex and fnmatch with multibyte characters */
	if (utf8) {
		static const char *const rp[][2] = { { "a.b", "a\xc3\xa9" "b" }, { "^.$", "\xe2\x82\xac" }, { "\xc3\xa9*", "\xc3\xa9\xc3\xa9x" },
			{ "[\xc3\xa9]", "caf\xc3\xa9" }, { "[^\xc3\xa9]*", "abc\xc3\xa9" }, { "[[:alpha:]]*", "Gr\xc3\xb6\xc3\x9f" "e12" },
			{ "[[:upper:]]", "a\xc3\x84" "b" }, { "x[^[:space:]]y", "x y x\xe2\x82\xac" "y" } };
		static const char *const fp[][2] = { { "?", "\xc3\xa9" }, { "caf?", "caf\xc3\xa9" }, { "*\xc3\xa9", "caf\xc3\xa9" },
			{ "[!a]", "\xc3\xa9" }, { "[[:alpha:]]", "\xc3\x9f" }, { "\xc3\x89", "\xc3\xa9" } };
		for (k = 0; k < sizeof rp / sizeof *rp; k++) {
			regex_t re;
			regmatch_t m[1];
			if (regcomp(&re, rp[k][0], 0)) { os("error\n"); continue; }
			if (regexec(&re, rp[k][1], 1, m, 0)) os("nomatch ");
			else { oi(m[0].rm_so); oc(','); oi(m[0].rm_eo); oc(' '); }
			regfree(&re);
		}
		for (k = 0; k < sizeof fp / sizeof *fp; k++) { oi(fnmatch(fp[k][0], fp[k][1], 0)); oi(fnmatch(fp[k][0], fp[k][1], FNM_CASEFOLD)); oc(' '); }
		onl();
	}
}

static void iconvs(void)
{
	static const char *const sets[] = { "ISO-8859-1", "ISO-8859-2", "ISO-8859-5", "ISO-8859-7", "ISO-8859-15", "KOI8-R",
		"CP1251", "CP1252", "IBM437", "IBM866", "EUC-JP", "SHIFT_JIS", "EUC-CN", "GBK", "GB18030", "EUC-KR", "BIG5",
		"UTF-16LE", "UTF-16BE", "UTF-32LE", "UCS-2BE", "UTF-8" };
	unsigned i, c;
	setlocale(LC_ALL, "C");
	for (i = 0; i < sizeof sets / sizeof *sets; i++) {
		iconv_t a = iconv_open(sets[i], "UTF-32LE"), b = iconv_open("UTF-32LE", sets[i]);
		unsigned long long he = 0, hd = 0;
		for (c = 1; c < 0x30000; c++) {
			char o[16], *ip, *op, o2[16], *ip2, *op2;
			size_t il, ol, r, il2, ol2;
			if (c - 0xd800 < 0x800) continue;
			ip = (char *)&c; il = 4; op = o; ol = sizeof o;
			iconv(a, 0, 0, 0, 0);
			r = iconv(a, &ip, &il, &op, &ol);
			h = he; H(r == (size_t)-1 ? 0 : 1);
			if (r != (size_t)-1) {
				char *q;
				for (q = o; q < op; q++) H((unsigned char)*q);
				he = h;
				/* and back */
				ip2 = o; il2 = (size_t)(op - o); op2 = o2; ol2 = sizeof o2;
				iconv(b, 0, 0, 0, 0);
				r = iconv(b, &ip2, &il2, &op2, &ol2);
				h = hd; H(r); H((size_t)(op2 - o2)); if (op2 - o2 >= 4) H(*(unsigned *)(void *)o2); hd = h;
			} else he = h;
		}
		os(sets[i]); oc(' '); ox(he); oc(' '); ox(hd); onl();
		iconv_close(a); iconv_close(b);
	}
	{
		iconv_t cd = iconv_open("ISO-2022-JP", "UTF-8");
		char in[] = "abc\xe6\x97\xa5\xe6\x9c\xac x\xc2\xa5" "y\n", out[64], *ip = in, *op = out;
		size_t il = strlen(in), ol = sizeof out;
		oi((long long)iconv(cd, &ip, &il, &op, &ol));
		iconv(cd, 0, 0, &op, &ol);
		for (ip = out; ip < op; ip++) { ox((unsigned char)*ip); oc(' '); }
		onl();
		iconv_close(cd);
	}
	oi(iconv_open("NO-SUCH", "UTF-8") == (iconv_t)-1); onl();
}

int main(void)
{
	const char *tl = getenv("TEST_LOCALES");
	char list[1024], *p, *q;
	one("C");
	one("POSIX");
	one("C.UTF-8");
	one("xx_YY.NOPE");
	if (tl && strlen(tl) < sizeof list) {
		strcpy(list, tl);
		for (p = list; *p; p = q) {
			q = strchr(p, ' ');
			if (q) *q++ = 0; else q = p + strlen(p);
			if (*p) one(p);
		}
	}
	iconvs();
	oflush();
	return 0;
}
