#include "libm.h"

long double hypotl(long double x, long double y)
{
	long double ax = __ld_abs(x), ay = __ld_abs(y), t;
	int e;
	if (ax - ax != 0 && ax == ax)
		return ax;              /* inf, even if y is NaN */
	if (ay - ay != 0 && ay == ay)
		return ay;
	if (x != x || y != y)
		return x + y;
	if (ax < ay) {
		t = ax; ax = ay; ay = t;
	}
	if (ay == 0)
		return ax;
	/* scale ax to [1, 2) */
	e = __ld_exp(ax) - 16383;
	if (e < -16382)
		e = -16382;
	ax = __x87_scale(ax, -e);
	ay = __x87_scale(ay, -e);
	return __math_l(__x87_scale(__x87_sqrt(ax * ax + ay * ay), e));
}

/* double: no scaling needed in long double */
double hypot(double x, double y)
{
	long double ax = x, ay = y;
	if (ax - ax != 0 || ay - ay != 0)
		return hypotl(x, y);
	return __math_d(__x87_sqrt(ax * ax + ay * ay));
}

float hypotf(float x, float y)
{
	return __math_f(hypotl(x, y));
}
