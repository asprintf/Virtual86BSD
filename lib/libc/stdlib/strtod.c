#include <stdlib.h>
#include <string.h>
#include "stdio_impl.h"

static long double strtox(const char *s, char **p, int prec)
{
	FILE f;
	long double y;
	off_t cnt;
	memset(&f, 0, sizeof f);
	f.buf = f.rpos = (unsigned char *)s;
	f.rend = (unsigned char *)-1;
	shlim(&f, 0);
	y = __floatscan(&f, prec, 1);
	cnt = shcnt(&f);
	if (p) *p = cnt ? (char *)s + cnt : (char *)s;
	return y;
}

float strtof(const char *__RESTRICT s, char **__RESTRICT p) { return (float)strtox(s, p, 0); }
double strtod(const char *__RESTRICT s, char **__RESTRICT p) { return (double)strtox(s, p, 1); }
long double strtold(const char *__RESTRICT s, char **__RESTRICT p) { return strtox(s, p, 2); }
double atof(const char *s) { return strtod(s, 0); }
