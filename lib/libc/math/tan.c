#include "libm.h"

long double tanl(long double x)
{
	long double r, t;
	if (__ld_nonfinite(x))
		return __math_invalid(x);
	if (__ld_abs(x) < 0x1p-32L)
		return __math_l(x);
	if (__rem_pio2l(x, &r) & 1) {
		t = __x87_tan(r);
		return -1 / t;
	}
	return __x87_tan(r);
}

__MATH_WRAP1(tan)
