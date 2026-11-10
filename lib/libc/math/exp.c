#include "libm.h"

/* e^x = 2^n e^r, n = rint(x log2 e), r = x - n ln 2 (ln 2 in two parts,
 * n * __LN2HI exact), e^r = 2^(r log2 e) by f2xm1 */
long double __expl(long double x)
{
	long double n = __x87_rint(x * __LOG2E);
	long double r = (x - n * __LN2HI) - n * __LN2LO;
	return __x87_scale(1 + __x87_2xm1(r * __LOG2E), n);
}

long double expl(long double x)
{
	if (x != x)
		return x + x;
	if (x > 11356.6L) {
		if (__ld_nonfinite(x))
			return x;
		return __math_oflow(0);
	}
	if (x < -11400) {
		if (__ld_nonfinite(x))
			return 0;
		return __math_uflow(0);
	}
	if (__ld_abs(x) < 0x1p-66L)
		return 1 + x;
	return __math_l(__expl(x));
}

__MATH_WRAP1(exp)
