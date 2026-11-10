#include "libm.h"

long double atanl(long double x)
{
	if (x != x)
		return x + x;
	if (__ld_abs(x) < 0x1p-32L)
		return __math_l(x);
	return __x87_atan2(x, 1.0L);
}

__MATH_WRAP1(atan)

/* fpatan follows C Annex F for zeros and infinities */
long double atan2l(long double y, long double x)
{
	if (x != x || y != y)
		return x + y;
	return __math_l(__x87_atan2(y, x));
}

__MATH_WRAP2(atan2)
