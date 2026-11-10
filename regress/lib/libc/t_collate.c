/* Differential: collation (strcoll, strxfrm, wcscoll, wcsxfrm and the _l
 * forms) in the C locales and the locales of TEST_LOCALES (LOCPATH, made
 * by run.sh), among them xx_XX, made from collate.def: a contraction,
 * expansions, ignored characters, a backward level, a position level.
 * Random strings over letters, accents, digits and punctuation; orders
 * by qsort with strcoll (ties broken by strcmp). */
#define _GNU_SOURCE
#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <regex.h>
#include <fnmatch.h>
#include "out.h"

#define NS 300

static const wchar_t tokens[] = L"aAbBcChsze E012 -.'\x00e1\x00c1\x00e0\x00e9\x00c9\x00e8\x00e6\x00df"
	L"\x0131\x0130iI\x00f1\x00fc\x00dc\x00f6\x0151\x00e7\x00f8\x00e5\x00e4\x00c4"
	L"\x043b\x041b\x0434\x0436\x0416\x044f\x0451\x0401\x4e2d\x6587\tx";

static char str[NS][64];
static wchar_t wstr[NS][32];

static int cmp(const void *a, const void *b)
{
	int r = strcoll(*(char *const *)a, *(char *const *)b);
	if (!r) r = strcmp(*(char *const *)a, *(char *const *)b);
	if (!r) r = *(char *const *)a < *(char *const *)b ? -1 : 1;   /* qsort need not be stable */
	return r;
}

static void hexs(const unsigned char *s, size_t n)
{
	size_t i;
	for (i = 0; i < n; i++) { oc("0123456789abcdef"[s[i] >> 4]); oc("0123456789abcdef"[s[i] & 15]); }
}

static void one(const char *name)
{
	char *r = setlocale(LC_ALL, name), *ord[NS], buf[4096];
	wchar_t wbuf[1024];
	wchar_t ok[sizeof tokens / sizeof *tokens];
	size_t nok = 0, i, j, n;
	locale_t loc;
	os("== "); os(name); os(": "); os(r ? r : "(null)"); onl();
	if (!r) return;
	/* the tokens this codeset has */
	for (i = 0; tokens[i]; i++) {
		char mb[16];
		mbstate_t st;
		memset(&st, 0, sizeof st);
		if (wcrtomb(mb, tokens[i], &st) != (size_t)-1) ok[nok++] = tokens[i];
	}
	oi((long long)nok); onl();
	for (i = 0; i < NS; i++) {
		size_t len = (size_t)(rng() % 7), k = 0;
		char *p = str[i];
		mbstate_t st;
		memset(&st, 0, sizeof st);
		for (j = 0; j < len; j++) {
			wchar_t w = ok[rng() % nok];
			/* words that differ a little: more of the same letters */
			if (rng() % 3 == 0) w = ok[rng() % 12];
			wstr[i][k++] = w;
			p += wcrtomb(p, w, &st);
		}
		*p = 0;
		wstr[i][k] = 0;
		ord[i] = str[i];
	}
	/* strxfrm */
	for (i = 0; i < NS; i++) {
		n = strxfrm(buf, str[i], sizeof buf);
		oi((long long)n); oc(':');
		if (n < sizeof buf) hexs((unsigned char *)buf, n);
		oi((long long)strxfrm(0, str[i], 0));
		onl();
	}
	/* strcoll and wcscoll of pairs, and their agreement with the transforms */
	for (i = 0; i < 4000; i++) {
		size_t a = rng() % NS, b = rng() % NS;
		int c = strcoll(str[a], str[b]), w = wcscoll(wstr[a], wstr[b]);
		oi(sgn(c)); oi(sgn(w)); oc(' ');
		if (i % 40 == 39) onl();
	}
	/* wcsxfrm */
	for (i = 0; i < NS; i += 3) {
		n = wcsxfrm(wbuf, wstr[i], sizeof wbuf / sizeof *wbuf);
		oi((long long)n); oc(':');
		for (j = 0; j < n && n < sizeof wbuf / sizeof *wbuf; j++) { ox((unsigned long long)wbuf[j]); oc('.'); }
		onl();
	}
	/* equivalence classes: [[=x=]] for some x, against every token */
	if (strcmp(name, "C.UTF-8")) {
		for (i = 0; i < nok && i < 40; i += 3) {
			char pat[32], one[16];
			mbstate_t st;
			regex_t re;
			size_t l;
			memset(&st, 0, sizeof st);
			if (ok[i] == L'-' || ok[i] == L']' || ok[i] < 0x20) continue;
			strcpy(pat, "[[=");
			l = wcrtomb(pat + 3, ok[i], &st);
			strcpy(pat + 3 + l, "=]]");
			if (regcomp(&re, pat, REG_NOSUB)) { os("regcomp failed"); onl(); continue; }
			for (j = 0; j < nok; j++) {
				memset(&st, 0, sizeof st);
				one[wcrtomb(one, ok[j], &st)] = 0;
				oi(regexec(&re, one, 0, 0, 0) == 0); oi(fnmatch(pat, one, 0) == 0);
			}
			regfree(&re);
			onl();
		}
	}
	/* an order */
	qsort(ord, NS, sizeof *ord, cmp);
	for (i = 0; i < NS; i++) { oi((long long)(ord[i] - str[0]) / 64); oc(' '); }
	onl();
	/* the _l forms in the C locale while this one is global, and back */
	loc = newlocale(LC_ALL_MASK, "C", 0);
	for (i = 0; i < 50; i++) {
		size_t a = rng() % NS, b = rng() % NS;
		oi(sgn(strcoll_l(str[a], str[b], loc)) == sgn(strcmp(str[a], str[b])));
		oi(sgn(wcscoll_l(wstr[a], wstr[b], loc)) == sgn(wcscmp(wstr[a], wstr[b])));
	}
	onl();
	freelocale(loc);
	loc = newlocale(LC_ALL_MASK, name, 0);
	setlocale(LC_ALL, "C");
	for (i = 0; i < 50; i++) {
		size_t a = rng() % NS, b = rng() % NS;
		oi(sgn(strcoll_l(str[a], str[b], loc)));
		n = strxfrm_l(buf, str[a], sizeof buf, loc);
		oi((long long)n % 10);
	}
	onl();
	freelocale(loc);
}

int main(void)
{
	const char *tl = getenv("TEST_LOCALES");
	char list[1024], *p;
	one("C");
	one("POSIX");
	one("C.UTF-8");
	if (tl && strlen(tl) < sizeof list) {
		strcpy(list, tl);
		for (p = strtok(list, " "); p; p = strtok(0, " ")) one(p);
	}
	oflush();
	return 0;
}
