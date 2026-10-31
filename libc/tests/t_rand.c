/* Differential: random/srandom/initstate/setstate, the rand48 family,
 * a64l/l64a and the multibyte functions in the POSIX locale. */
#define _XOPEN_SOURCE 800
#include <stdlib.h>
#include "out.h"

int main(void)
{
	static char st1[256], st2[64], st3[32], st4[8];
	unsigned short x[3] = { 1, 2, 3 }, p[7] = { 5, 6, 7, 8, 9, 10, 11 }, *sp;
	char *old;
	int i;
	long v;
	wchar_t wc, wb[8];
	char mb[8];

	/* unseeded */
	for (i = 0; i < 5; i++) { oi(random()); onl(); }
	srandom(12345);
	for (i = 0; i < 40; i++) { oi(random()); onl(); }
	old = initstate(7, st1, sizeof st1);
	for (i = 0; i < 40; i++) { oi(random()); onl(); }
	initstate(9, st2, sizeof st2);
	for (i = 0; i < 20; i++) { oi(random()); onl(); }
	initstate(11, st3, sizeof st3);
	for (i = 0; i < 20; i++) { oi(random()); onl(); }
	initstate(13, st4, sizeof st4);
	for (i = 0; i < 10; i++) { oi(random()); onl(); }
	setstate(st1);
	for (i = 0; i < 10; i++) { oi(random()); onl(); }
	setstate(old);
	for (i = 0; i < 10; i++) { oi(random()); onl(); }

	/* rand48 (the unseeded state is unspecified: glibc starts from 0,
	 * this library from the traditional 0x1234abcd330e) */
	srand48(987654);
	for (i = 0; i < 10; i++) { oi(lrand48()); oc(' '); oi(mrand48()); oc(' '); oi((long long)(drand48() * 1e15)); onl(); }
	for (i = 0; i < 5; i++) { oi(nrand48(x)); oc(' '); oi(jrand48(x)); oc(' '); oi((long long)(erand48(x) * 1e15)); onl(); }
	sp = seed48(x); oi(sp[0]); oc(' '); oi(sp[1]); oc(' '); oi(sp[2]); onl();
	for (i = 0; i < 5; i++) { oi(lrand48()); onl(); }
	lcong48(p);
	for (i = 0; i < 5; i++) { oi(lrand48()); oc(' '); oi(mrand48()); onl(); }

	/* a64l/l64a (POSIX sign-extends a64l's 32-bit result; glibc does not) */
	for (i = 0; i < 2000; i++) {
		v = (long)(rng() & 0xffffffff);
		if (i < 20) v = i * 7;
		os(l64a(v)); oc(' '); oi(a64l(l64a(v)) & 0xffffffff); onl();
	}
	oi(a64l("")); oc(' '); oi(a64l("zz/.1") & 0xffffffff); oc(' '); oi(a64l("abcdefghij") & 0xffffffff); onl();

	/* multibyte, POSIX locale */
	oi(MB_CUR_MAX); onl();
	oi(mblen("a", 1)); oi(mblen("", 1)); oi(mblen(0, 0)); onl();
	oi(mbtowc(&wc, "A", 1)); oc(' '); oi(wc); onl();
	oi(wctomb(mb, L'z')); oc(' '); oi(mb[0]); onl();
	oi((long long)mbstowcs(wb, "hello", 8)); oc(' '); oi(wb[4]); oc(' '); oi(wb[5]); onl();
	oi((long long)mbstowcs(0, "hello", 0)); onl();
	oi((long long)wcstombs(mb, L"abc", 8)); os(mb); onl();
	oi((long long)wcstombs(0, L"abcd", 0)); onl();
	oflush();
	return 0;
}
