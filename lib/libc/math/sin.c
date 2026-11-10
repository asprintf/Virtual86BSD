#include "libm.h"

long double sinl(long double x)
{
	long double r;
	if (__ld_nonfinite(x))
		return __math_invalid(x);
	if (__ld_abs(x) < 0x1p-32L)
		return __math_l(x);
	switch (__rem_pio2l(x, &r)) {
	case 0: return __x87_sin(r);
	case 1: return __x87_cos(r);
	case 2: return -__x87_sin(r);
	default: return -__x87_cos(r);
	}
}

__MATH_WRAP1(sin)
