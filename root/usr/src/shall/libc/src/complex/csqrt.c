#include "complex_impl.h"

/* Kahan's algorithm: t = sqrt((|x| + |z|)/2), then the other part y/(2t) */
void __csqrt_parts(long double x, long double y, long double *re, long double *im)
{
	long double t, ax = fabsl(x);
	int scale = 0;
	if (y - y != 0 && y == y) {             /* y infinite */
		*re = 1.0L / 0.0L;
		*im = y;
		return;
	}
	if (x != x) {
		*re = x;
		*im = y - y + x;
		return;
	}
	if (ax - ax != 0) {                     /* x infinite */
		if (x > 0) {
			*re = x;
			*im = y == y ? copysignl(0, y) : y;
		} else {
			*re = y == y ? 0 : y;
			*im = copysignl(ax, y);
		}
		return;
	}
	if (y != y) {
		*re = *im = y;
		return;
	}
	if (x == 0 && y == 0) {
		*re = 0;
		*im = y;
		return;
	}
	if (ax > 0x1p16380L || fabsl(y) > 0x1p16380L) {
		ax *= 0x1p-4L; y *= 0x1p-4L;
		scale = 2;
	} else if (ax < 0x1p-16300L && fabsl(y) < 0x1p-16300L) {
		ax *= 0x1p64L; y *= 0x1p64L;
		scale = -32;
	}
	t = sqrtl((ax + hypotl(ax, y)) / 2);
	if (x >= 0) {
		*re = t;
		*im = y / (2 * t);
	} else {
		*re = fabsl(y) / (2 * t);
		*im = copysignl(t, y);
	}
	if (scale) {
		*re = ldexpl(*re, scale);
		*im = ldexpl(*im, scale);
	}
}

long double _Complex csqrtl(long double _Complex z)
{
	long double re, im;
	__csqrt_parts(__cre(z), __cim(z), &re, &im);
	return __cpack(re, im);
}
__CWRAP(csqrt)
