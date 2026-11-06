#include "libm.h"

/* an approximation 2^(log2|x| / 3) by the x87, then one Halley step
 * y (y^3 + 2x) / (2y^3 + x) on a scaled x, which triples the accuracy */
long double cbrtl(long double x)
{
	long double ax = __ld_abs(x), y, y3, n, m;
	int e;
	if (x == 0 || __ld_nonfinite(x))
		return x + x;
	/* ax = m 2^(3n), m in [1, 8) */
	e = __ld_exp(ax) - 16383;
	if (e < -16382) {               /* subnormal */
		ax *= 0x1p66L;
		e = __ld_exp(ax) - 16383 - 66;
	}
	n = (long double)((e + 3 * 6000) / 3 - 6000);
	m = __x87_scale(ax, -3 * n);
	y = __x87_scale(1 + __x87_2xm1(__x87_yl2x(m, 1.0L / 3) - 1), 1);
	y3 = y * y * y;
	y = y * (y3 + 2 * m) / (2 * y3 + m);
	y = __x87_scale(y, n);
	return x < 0 ? -y : y;
}

__MATH_WRAP1(cbrt)
