#include "libm.h"

long double asinhl(long double x)
{
	long double ax = __ld_abs(x), t, r;
	if (__ld_nonfinite(x))
		return x + x;
	if (ax < 0x1p-32L)
		return __math_l(x);
	if (ax > 0x1p34L)
		r = __x87_yl2x(ax, __LN2) + __LN2;
	else if (ax > 2)
		r = __x87_yl2x(2 * ax + 1 / (__x87_sqrt(ax * ax + 1) + ax), __LN2);
	else {
		t = ax * ax;
		r = __log1pl(ax + t / (1 + __x87_sqrt(1 + t)));
	}
	return x < 0 ? -r : r;
}

long double acoshl(long double x)
{
	long double t;
	if (x != x)
		return x + x;
	if (x < 1)
		return __math_invalid(x);
	if (__ld_nonfinite(x))
		return x;
	if (x > 0x1p34L)
		return __x87_yl2x(x, __LN2) + __LN2;
	if (x > 2)
		return __x87_yl2x(2 * x - 1 / (x + __x87_sqrt(x * x - 1)), __LN2);
	t = x - 1;
	return __log1pl(t + __x87_sqrt(2 * t + t * t));
}

long double atanhl(long double x)
{
	long double ax = __ld_abs(x), r;
	if (x != x)
		return x + x;
	if (ax > 1)
		return __math_invalid(x);
	if (ax == 1)
		return __math_divzero(x < 0);
	if (ax < 0x1p-32L)
		return __math_l(x);
	if (ax < 0.5L)
		r = __log1pl(2 * ax + 2 * ax * ax / (1 - ax)) / 2;
	else
		r = __log1pl(2 * ax / (1 - ax)) / 2;
	return x < 0 ? -r : r;
}

__MATH_WRAP1(asinh)
__MATH_WRAP1(acosh)
__MATH_WRAP1(atanh)
