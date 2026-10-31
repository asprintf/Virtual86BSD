/* x * 2^n: fscale is exact in long double, so double and float results
 * are rounded once */
#include "libm.h"

long double scalblnl(long double x, long n)
{
	long double r;
	if (x == 0 || x - x != 0)
		return x + x;
	r = __x87_scale(x, (long double)n);
	if (r == 0)
		errno = ERANGE;
	return __math_l(r);
}

long double scalbnl(long double x, int n) { return scalblnl(x, n); }
long double ldexpl(long double x, int n) { return scalblnl(x, n); }

static long double scale(long double x, long n)
{
	if (x == 0 || x - x != 0)
		return x + x;
	return __x87_scale(x, (long double)n);
}

double scalbln(double x, long n) { return __math_d(scale(x, n)); }
double scalbn(double x, int n) { return __math_d(scale(x, n)); }
double ldexp(double x, int n) { return __math_d(scale(x, n)); }
float scalblnf(float x, long n) { return __math_f(scale(x, n)); }
float scalbnf(float x, int n) { return __math_f(scale(x, n)); }
float ldexpf(float x, int n) { return __math_f(scale(x, n)); }
