#include "libm.h"

long double frexpl(long double x, int *e)
{
	union __ldbits u = {x};
	int ee = u.i.se & 0x7fff;
	if (x == 0 || ee == 0x7fff) {
		*e = 0;
		return x + x;
	}
	if (ee == 0) {
		u.f *= 0x1p64L;
		ee = (u.i.se & 0x7fff) - 64;
	}
	*e = ee - 16382;
	u.i.se = (u.i.se & 0x8000) | 16382;
	return u.f;
}

double frexp(double x, int *e) { return frexpl(x, e); }
float frexpf(float x, int *e) { return frexpl(x, e); }
