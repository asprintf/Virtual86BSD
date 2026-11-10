#include "libm.h"

/* small |x|: f2xm1 directly; else e^x - 1 = 2^n (f + (1 - 2^-n)) with
 * e^r = 1 + f as in exp(), exact but for the one addition */
long double __expm1l(long double x)
{
	long double n, r, f;
	if (__ld_abs(x) <= 0.5L)
		return __x87_2xm1(x * __LOG2E);
	if (x < -46)
		return -1 + 0x1p-100L;
	n = __x87_rint(x * __LOG2E);
	r = (x - n * __LN2HI) - n * __LN2LO;
	f = __x87_2xm1(r * __LOG2E);
	if (n > 64)
		return __x87_scale(1 + f, n);
	return __x87_scale(f + (1 - __x87_scale(1, -n)), n);
}

long double expm1l(long double x)
{
	if (x != x)
		return x + x;
	if (x > 11356.6L) {
		if (__ld_nonfinite(x))
			return x;
		return __math_oflow(0);
	}
	if (__ld_nonfinite(x))
		return -1;
	if (__ld_abs(x) < 0x1p-64L)
		return __math_l(x);
	return __expm1l(x);
}

__MATH_WRAP1(expm1)
