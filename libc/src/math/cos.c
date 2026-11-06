#include "libm.h"

long double cosl(long double x)
{
	long double r;
	if (__ld_nonfinite(x))
		return __math_invalid(x);
	switch (__rem_pio2l(x, &r)) {
	case 0: return __x87_cos(r);
	case 1: return -__x87_sin(r);
	case 2: return -__x87_cos(r);
	default: return __x87_sin(r);
	}
}

__MATH_WRAP1(cos)
