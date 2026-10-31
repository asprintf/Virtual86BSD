/* fmod, remainder and remquo are exact: fprem and fprem1 compute them
 * in long double, also for the double and float arguments */
#include "libm.h"

static long double rem(long double x, long double y, int *q, int ieee)
{
	int b, neg = (x < 0) != (y < 0);
	*q = 0;
	if (x != x || y != y)
		return x + y;
	if (x - x != 0 || y == 0)
		return __math_invalid(x - x != 0 ? x : y);
	if (y - y != 0 || x == 0)
		return x;
	x = ieee ? __x87_remainder(x, y, &b) : __x87_fmod(x, y, &b);
	*q = neg ? -b : b;
	return x;
}

long double fmodl(long double x, long double y) { int q; return rem(x, y, &q, 0); }
double fmod(double x, double y) { int q; return rem(x, y, &q, 0); }
float fmodf(float x, float y) { int q; return rem(x, y, &q, 0); }

long double remainderl(long double x, long double y) { int q; return rem(x, y, &q, 1); }
double remainder(double x, double y) { int q; return rem(x, y, &q, 1); }
float remainderf(float x, float y) { int q; return rem(x, y, &q, 1); }

long double remquol(long double x, long double y, int *q) { return rem(x, y, q, 1); }
double remquo(double x, double y, int *q) { return rem(x, y, q, 1); }
float remquof(float x, float y, int *q) { return rem(x, y, q, 1); }
