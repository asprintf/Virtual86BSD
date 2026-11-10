#include "libm.h"

long double exp2l(long double x)
{
	long double n;
	if (x != x)
		return x + x;
	if (x >= 16384) {
		if (__ld_nonfinite(x))
			return x;
		return __math_oflow(0);
	}
	if (x < -16446) {
		if (__ld_nonfinite(x))
			return 0;
		return __math_uflow(0);
	}
	n = __x87_rint(x);
	return __math_l(__x87_scale(1 + __x87_2xm1(x - n), n));
}

__MATH_WRAP1(exp2)
