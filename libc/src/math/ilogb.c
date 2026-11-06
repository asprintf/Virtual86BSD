#include "libm.h"
#include <limits.h>
#include <fenv.h>

/* the exponent of a finite nonzero x, subnormals included */
static int expo(long double x)
{
	int e = __ld_exp(x);
	if (e == 0)
		e = __ld_exp(x * 0x1p64L) - 64;
	return e - 16383;
}

int ilogbl(long double x)
{
	int r;
	if (x != x)
		r = FP_ILOGBNAN;
	else if (x == 0)
		r = FP_ILOGB0;
	else if (__ld_nonfinite(x))
		r = INT_MAX;
	else
		return expo(x);
	errno = EDOM;
	feraiseexcept(FE_INVALID);
	return r;
}

int ilogb(double x) { return ilogbl(x); }
int ilogbf(float x) { return ilogbl(x); }

long double logbl(long double x)
{
	if (x != x)
		return x + x;
	if (x == 0)
		return __math_divzero(1);     /* a pole error: -HUGE_VALL, ERANGE, FE_DIVBYZERO (POSIX) */
	if (__ld_nonfinite(x))
		return __ld_abs(x);
	return expo(x);
}

double logb(double x) { return logbl(x); }
float logbf(float x) { return logbl(x); }
