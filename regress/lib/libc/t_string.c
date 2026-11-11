#include <string.h>
#include <strings.h>
#include "out.h"

#define N 200
static char A[N + 64], B[N + 64];

static void randstr(char *s, size_t n, int alpha)
{ size_t i; for (i = 0; i < n; i++) s[i] = (char)("abcAB\x80\xff-"[rng() % alpha]); s[n] = 0; }

static void offs(const void *base, const void *p) { if (p) oi((const char *)p - (const char *)base); else os("null"); oc(' '); }

int main(void)
{
	int it;
	for (it = 0; it < 20000; it++) {
		int alpha = 2 + (int)(rng() % 7);
		size_t la = rng() % 40, lb = rng() % 8, n = rng() % 50;
		char *a = A + rng() % 9, *b = B + rng() % 9;
		char d1[N + 64], d2[N + 64];
		char *sp, *tok, *save;
		randstr(a, la, alpha); randstr(b, lb, alpha);
		offs(a, memchr(a, b[0], la)); offs(a, strchr(a, b[0])); offs(a, strrchr(a, b[0]));
		offs(a, strchr(a, 0)); offs(a, strrchr(a, 0));
		oi(sgn(memcmp(a, b, la < lb ? la : lb))); oc(' ');
		oi(sgn(strcmp(a, b))); oc(' '); oi(sgn(strncmp(a, b, n % 6))); oc(' ');
		oi(sgn(strcasecmp(a, b))); oc(' '); oi(sgn(strncasecmp(a, b, n % 6))); oc(' ');
		ou(strlen(a)); oc(' '); ou(strnlen(a, n)); oc(' ');
		ou(strspn(a, b)); oc(' '); ou(strcspn(a, b)); oc(' '); offs(a, strpbrk(a, b));
		offs(a, strstr(a, b)); offs(a, memmem(a, la, b, lb));
		offs(a, memmem(a, la, b, n % 3));
		/* copies */
		memset(d1, 'Z', sizeof d1);
		offs(d1, stpcpy(d1, a)); os(d1); oc(' ');
		memset(d1, 'Z', sizeof d1);
		offs(d1, stpncpy(d1, a, n)); { size_t k; for (k = 0; k < n + 2; k++) ox((unsigned char)d1[k]); } oc(' ');
		memset(d1, 'Q', sizeof d1); strcpy(d1, b); strncat(d1, a, n % 10); os(d1); oc(' ');
		memset(d1, 'Q', sizeof d1); ou(strlcpy(d1, a, n % 12)); if (n % 12) os(d1); oc(' ');
		strcpy(d1, b); ou(strlcat(d1, a, n % 20)); os(d1); oc(' ');
		memset(d1, 'Q', sizeof d1); offs(d1, memccpy(d1, a, b[0], la)); oc(' ');
		/* memmove both directions */
		memcpy(d1, a, la); memcpy(d2, a, la);
		{ size_t o = rng() % 8, k = la > o ? la - o : 0;
		  memmove(d1 + o, d1, k); memmove(d2, d2 + o, k);
		  { size_t q; for (q = 0; q < la; q++) { oc(d1[q]); oc(d2[q]); } } }
		oc(' ');
		/* strtok_r */
		strcpy(d1, a);
		for (tok = strtok_r(d1, b, &save); tok; tok = strtok_r(0, b, &save)) { os(tok); oc('|'); }
		strcpy(d2, a); sp = strtok(d2, "a");
		while (sp) { os(sp); oc(','); sp = strtok(0, "a"); }
		onl();
	}
	/* long needles for the Two-Way path */
	for (it = 0; it < 3000; it++) {
		char h[600], nd[80];
		int alpha = 2 + (int)(rng() % 3);
		size_t lh = rng() % 599, ln = 1 + rng() % 79, k;
		randstr(h, lh, alpha);
		if (lh > ln && rng() % 2) { k = rng() % (lh - ln); memcpy(nd, h + k, ln); nd[ln] = 0; }
		else randstr(nd, ln, alpha);
		offs(h, strstr(h, nd)); offs(h, memmem(h, lh, nd, ln));
		if (it % 10 == 9) onl();
	}
	oi(ffs(0)); oi(ffs(1)); oi(ffs(0x80000000)); oi(ffsl(1L << 40)); oi(ffsll(-1LL)); onl();
	/* the messages: as glibc's (strsignal of the real-time signals differs: SIGRTMIN) */
	/* (0-133: this library's errnos; newer glibc has messages beyond) */
	for (it = 0; it < 134; it++) { os(strerror(it)); onl(); }
	for (it = 0; it < 32; it++) { os(strsignal(it)); onl(); }
	os(strsignal(65)); onl(); os(strsignal(-3)); onl();
	oflush();
	return 0;
}
