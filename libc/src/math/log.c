#include "libm.h"

/* c * log2(x) for x > 0 finite: fyl2xp1 near 1, where x - 1 is exact */
static long double clog2(long double x, long double c)
{
	if (x > 0.71L && x < 1.29L)
		return __x87_yl2xp1(x - 1, c);
	return __x87_yl2x(x, c);
}

static long double logx(long double x, long double c)
{
	if (x != x)
		return x + x;
	if (x == 0)
		return __math_divzero(1);
	if (x < 0)
		return __math_invalid(x);
	if (__ld_nonfinite(x))
		return x;
	return clog2(x, c);
}

long double logl(long double x) { return logx(x, __LN2); }
long double log2l(long double x) { return logx(x, 1); }
long double log10l(long double x) { return logx(x, __LOG10_2); }

__MATH_WRAP1(log)
__MATH_WRAP1(log2)
__MATH_WRAP1(log10)
