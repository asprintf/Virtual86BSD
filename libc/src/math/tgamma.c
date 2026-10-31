/* tgamma in long double:
 *   x >= 12:  sqrt(2 pi) x^(x - 1/2) e^-x e^(Stirling tail), the power
 *             as (x^((x - 1/2)/2) e^-x) x^((x - 1/2)/2) against overflow
 *   0 < x < 12: Gamma(x + n) / (x (x + 1) ... (x + n - 1))
 *   x < 0: pi / (sin(pi x) Gamma(1 - x))
 */
#include "libm.h"

#define SQRT_2PI 2.50662827463100050241576528481104525L

static long double gamma_pos(long double x)      /* x > 0, finite, < 1756 */
{
	long double p, q = 1;
	if (x < 12) {
		while (x < 12) {
			q *= x;
			x += 1;
		}
	}
	p = powl(x, (x - 0.5L) / 2);
	return p * __expl(-x) * p * SQRT_2PI * __expl(__stirling_tail(x)) / q;
}

long double tgammal(long double x)
{
	long double s, g;
	if (x != x)
		return x + x;
	if (x - x != 0)
		return x > 0 ? x : __math_invalid(x);
	if (x == 0)
		return __math_divzero(__signbitl(x));
	if (x > 0) {
		if (x > 1755.5L)
			return __math_oflow(0);
		if (x < 0x1p-70L)
			return __math_l(1 / x);
		return __math_l(gamma_pos(x));
	}
	if (__x87_rint(x) == x)
		return __math_invalid(x);
	s = __sinpil(x);
	if (x < -1754)
		return __math_uflow(s < 0);
	if (x > -0x1p-70L)
		return __math_l(1 / x);
	g = gamma_pos(1 - x);
	return __math_l(__PI / (s * g));
}

__MATH_WRAP1(tgamma)
