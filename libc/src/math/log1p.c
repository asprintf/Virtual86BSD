#include "libm.h"

/* log(1 + x): fyl2xp1 for |x| < 1 - sqrt(2)/2; else log(u) for
 * u = 1 + x rounded, corrected by (x - (u - 1)) / u */
long double __log1pl(long double x)
{
	long double u;
	if (__ld_abs(x) < 0.29L)
		return __x87_yl2xp1(x, __LN2);
	u = 1 + x;
	return __x87_yl2x(u, __LN2) + (x - (u - 1)) / u;
}

long double log1pl(long double x)
{
	if (x != x)
		return x + x;
	if (x == -1)
		return __math_divzero(1);
	if (x < -1)
		return __math_invalid(x);
	if (__ld_nonfinite(x))
		return x;
	if (__ld_abs(x) < 0x1p-64L)
		return __math_l(x);
	return __log1pl(x);
}

__MATH_WRAP1(log1p)
