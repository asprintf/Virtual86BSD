#include "libm.h"

/* asin x = atan2(x, sqrt((1 - x)(1 + x))) */
long double asinl(long double x)
{
	if (x != x)
		return x + x;
	if (__ld_abs(x) > 1)
		return __math_invalid(x);
	if (__ld_abs(x) < 0x1p-32L)
		return __math_l(x);
	return __x87_atan2(x, __x87_sqrt((1 - x) * (1 + x)));
}

__MATH_WRAP1(asin)

/* acos x = atan2(sqrt((1 - x)(1 + x)), x) */
long double acosl(long double x)
{
	if (x != x)
		return x + x;
	if (__ld_abs(x) > 1)
		return __math_invalid(x);
	return __x87_atan2(__x87_sqrt((1 - x) * (1 + x)), x);
}

__MATH_WRAP1(acos)
