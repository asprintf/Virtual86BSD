/* Differential: the multibyte codesets other than UTF-8 as locale
 * codesets (EUC-JP, SHIFT_JIS, EUC-KR, GB2312, GBK, GB18030, BIG5; the
 * locales of TEST_LOCALES that have them): mbrtowc over every byte and
 * byte pair (and GB18030's four-byte forms), fed whole and byte by byte;
 * wcrtomb over every code point; btowc/wctob; mbstowcs/wcstombs; the
 * classes and widths of the characters; regex and fnmatch with
 * multibyte patterns (also trail bytes in the ASCII range); printf %ls,
 * %lc and wide printf widths; wide streams; iconv from "". */
#define _GNU_SOURCE
#include <limits.h>
#include <ctype.h>
#include <locale.h>
#include <langinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <regex.h>
#include <fnmatch.h>
#include <iconv.h>
#include "out.h"

static unsigned long long h;
static void H(unsigned long long v) { h = h * 1000003 + v; }

static void conv(const char *s, size_t n)
{
	mbstate_t st;
	wchar_t w;
	size_t r, i;
	/* whole */
	memset(&st, 0, sizeof st);
	w = 0x12345;
	r = mbrtowc(&w, s, n, &st);
	H(r); H((unsigned long long)(r < 5 ? w : 0)); H(mbsinit(&st));
	/* byte by byte */
	memset(&st, 0, sizeof st);
	for (i = 0; i < n; i++) {
		w = 0x12345;
		r = mbrtowc(&w, s + i, 1, &st);
		H(r);
		if (r != (size_t)-2) { H((unsigned long long)(r < 5 ? w : 0)); break; }
	}
}

/* a string of characters by the code points given; to bytes in the locale */
static int enc(char *out, const wchar_t *ws)
{
	size_t r = wcstombs(out, ws, 256);
	if (r == (size_t)-1) return 0;
	return 1;
}

static void one(const char *name)
{
	char *r = setlocale(LC_ALL, name), buf[1024];
	char s[8];
	unsigned a, b, c;
	os("== "); os(name); os(": "); os(r ? r : "(null)"); onl();
	if (!r) return;
	os(nl_langinfo(CODESET)); oc(' '); oi((long long)MB_CUR_MAX); onl();

	/* every byte, every pair */
	h = 0;
	for (a = 0; a < 256; a++) {
		s[0] = (char)a; s[1] = 0;
		conv(s, 1);
		H(btowc((int)a));
		for (b = 0; b < 256; b++) {
			s[0] = (char)a; s[1] = (char)b; s[2] = 'x'; s[3] = 0;
			conv(s, 3);
		}
	}
	ox(h); onl();
	/* GB18030's four bytes, and three-byte EUC-JP */
	h = 0;
	for (a = 0x80; a < 0x100; a += 3)
		for (b = 0x28; b < 0x40; b++)
			for (c = 0x80; c < 0x100; c += 5) {
				s[0] = (char)a; s[1] = (char)b; s[2] = (char)c; s[3] = '3'; s[4] = 0;
				conv(s, 4);
				s[1] = (char)(a ^ 0x55); s[3] = 0;
				conv(s, 3);
			}
	ox(h); onl();
	/* every code point */
	h = 0;
	for (c = 0; c < 0x110000; c++) {
		mbstate_t st;
		size_t k;
		memset(&st, 0, sizeof st);
		k = wcrtomb(buf, (wchar_t)c, &st);
		H(k);
		if (k != (size_t)-1) for (a = 0; a < k; a++) H((unsigned char)buf[a]);
		H((unsigned)wctob((wint_t)c));
		if (c < 0x30000 && k != (size_t)-1) {
			H(!!iswalpha((wint_t)c) | !!iswupper((wint_t)c) << 1 | !!iswprint((wint_t)c) << 2 | !!iswspace((wint_t)c) << 3);
			H((unsigned)wcwidth((wchar_t)c));
			H(towupper((wint_t)c));
		}
	}
	ox(h); onl();
	/* the bytes: classes */
	for (a = 0x80; a < 0x100; a++) { oi(!!isalpha((int)a)); oi(!!isprint((int)a)); }
	onl();

	/* strings */
	{
		static const wchar_t *const ws[] = {
			L"abc", L"\x65e5\x672c\x8a9e", L"\x30bd\x30fc\x30b9\\x", L"\x8868\x793a[a]", L"\xff8a\xff9d\xff76\xff78",
			L"\xd55c\xad6d\xc5b4", L"\x4e2d\x6587\x5b57", L"\x7e41\x9ad4\x5b57", L"\x00e9t\x00e9", L"\x20ac\x00a5\x203e",
			L"\x0391\x0430\x2460", L"\x9fa5\xe000\x2e81",
		};
		static const wchar_t *const pats[] = {
			L"\x65e5.", L"[\x672c\x8a9e]", L"^\x30bd*", L"\x8868\x793a", L"[[:alpha:]]+", L"\x30bd\\\\", L".$",
			L"[^a-z]", L"\x4e2d|\x7e41", L"\\(.\\)\\1", L"[\x30bd-\x30fc]", L"\xc5b4$",
		};
		size_t i, j;
		char sb[256], pb[256];
		for (i = 0; i < sizeof ws / sizeof *ws; i++) {
			wchar_t back[64];
			size_t n;
			if (!enc(sb, ws[i])) { os("-"); onl(); continue; }
			n = strlen(sb);
			oi((long long)n); oc(' ');
			oi((long long)mbstowcs(back, sb, 64)); oc(' ');
			oi((long long)!wcscmp(back, ws[i])); oc(' ');
			oi((long long)mbstowcs(0, sb, 0)); oc(' ');
			/* printf */
			snprintf(buf, sizeof buf, "[%ls|%5ls|%-4.2ls|%lc]", ws[i], ws[i], ws[i], ws[i][0]);
			os(buf); oc(' ');
			{
				wchar_t wb[128];
				int k = swprintf(wb, 128, L"[%s|%6s|%.2s|%-5ls|%c]", sb, sb, sb, ws[i], 'x');
				oi(k); oc(' ');
				if (k > 0) for (j = 0; j < (size_t)k; j++) { ox((unsigned long long)wb[j]); oc('.'); }
			}
			onl();
			for (j = 0; j < sizeof pats / sizeof *pats; j++) {
				regex_t re;
				regmatch_t m[2];
				int e;
				if (!enc(pb, pats[j])) { oc('-'); continue; }
				e = regcomp(&re, pb, j == 8 ? REG_EXTENDED : j == 4 ? REG_EXTENDED : 0);
				if (e) { oc('E'); oi(e); oc(' '); continue; }
				e = regexec(&re, sb, 2, m, 0);
				if (e) oc('n');
				else { oi((long long)m[0].rm_so); oc(','); oi((long long)m[0].rm_eo); oc(','); oi((long long)m[1].rm_so); }
				regfree(&re);
				oi(fnmatch(pb, sb, 0) == 0);
				oc(' ');
			}
			onl();
		}
	}
	/* wide streams */
	{
		FILE *f = tmpfile();
		wint_t c2;
		if (f) {
			/* only the characters of the codeset (glibc writes '?' for
			 * the others, where POSIX asks for EILSEQ) */
			static const wchar_t wsc[] = L"\x65e5\x672c a\xff8a\xd55c\x4e2d\x00e9\x20ac\n";
			size_t q;
			for (q = 0; wsc[q]; q++) {
				char mb[MB_LEN_MAX];
				mbstate_t st;
				memset(&st, 0, sizeof st);
				if (wcrtomb(mb, wsc[q], &st) != (size_t)-1) fputwc(wsc[q], f);
			}
			rewind(f);
			while ((c2 = fgetwc(f)) != WEOF) { ox(c2); oc(' '); }
			oi(ferror(f) != 0);
			fclose(f);
		}
		onl();
	}
	/* iconv from the locale's codeset */
	{
		iconv_t cd = iconv_open("UTF-8", "");
		char in[64], out[64], *ip = in, *op = out;
		size_t il, ol = sizeof out;
		if (cd != (iconv_t)-1 && enc(in, L"\x65e5\x672c-\x4e2d")) {
			il = strlen(in);
			oi((long long)iconv(cd, &ip, &il, &op, &ol));
			oc(' ');
			oi((long long)(sizeof out - ol));
			iconv_close(cd);
		}
		onl();
	}
}

int main(void)
{
	const char *tl = getenv("TEST_LOCALES");
	char list[2048], *p;
	if (tl && strlen(tl) < sizeof list) {
		strcpy(list, tl);
		for (p = strtok(list, " "); p; p = strtok(0, " ")) {
			setlocale(LC_ALL, p);
			if (MB_CUR_MAX > 1 && strcmp(nl_langinfo(CODESET), "UTF-8")) one(p);
		}
	}
	oflush();
	return 0;
}
