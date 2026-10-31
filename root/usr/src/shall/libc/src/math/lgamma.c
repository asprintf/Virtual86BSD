/* lgamma: log |Gamma(x)| in long double; the sign of Gamma(x) goes to
 * signgam (through __signgam, see arch/x86_64/x87.s).
 *   |t| <= 1/2:  lgamma(2 + t) = (1 - gamma) t + sum_k>=2 (-1)^k (zeta(k) - 1) t^k / k
 *   lgamma(1 + t) = lgamma(2 + t) - log1p(t)
 *   2.5 <= x < 13: down to [1.5, 2.5) by Gamma(x + 1) = x Gamma(x)
 *   x >= 13: Stirling's series
 *   x < 0: Gamma(x) Gamma(1 - x) = pi / sin(pi x)
 */
#include "libm.h"
#include "zeta.h"

extern int __signgam;

/* lgamma(2 + t), |t| <= 1/2 */
static long double l2(long double t)
{
	long double s = 0;
	int k;
	for (k = 41; k >= 2; k--)
		s = s * t + (k & 1 ? -__zm1[k - 2] : __zm1[k - 2]) / k;
	return t * ((1 - __EGAMMA) + t * s);
}

long double __stirling_tail(long double x);

#define HALF_LOG_2PI 0.918938533204672741780329736405617640L

static long double lgamma_pos(long double x)     /* x > 0 finite */
{
	long double p, t;
	int n;
	if (x < 0.5L)
		return l2(x) - __log1pl(x) - __x87_yl2x(x, __LN2);
	if (x < 1.5L) {
		t = x - 1;
		return l2(t) - __log1pl(t);
	}
	if (x < 2.5L)
		return l2(x - 2);
	if (x < 13) {               /* x = y + n, y in [1.5, 2.5) */
		n = (int)(x - 1.5L);
		t = x - n;          /* exact */
		p = t;
		while (--n > 0)
			p *= t + n;
		return l2(t - 2) + __x87_yl2x(p, __LN2);
	}
	return (x - 0.5L) * (__x87_yl2x(x, __LN2) - 1) - 0.5L + HALF_LOG_2PI + __stirling_tail(x);
}

long double __lgammal_r(long double x, int *sign)
{
	long double s;
	*sign = 1;
	if (x != x)
		return x + x;
	if (x - x != 0)
		return __ld_abs(x);
	if (x == 0) {
		if (__signbitl(x))
			*sign = -1;
		return __math_divzero(0);
	}
	if (x > 0)
		return lgamma_pos(x);
	if (x > -1) {               /* Gamma(x) = Gamma(1 + x) / x */
		*sign = -1;
		return lgamma_pos(1 + x) - __x87_yl2x(-x, __LN2);
	}
	s = __sinpil(x);
	if (s == 0)
		return __math_divzero(0);
	if (s < 0)
		*sign = -1;
	/* log(pi / |sin(pi x)|) - lgamma(1 - x) */
	return __x87_yl2x(__PI / __ld_abs(s), __LN2) - lgamma_pos(1 - x);
}

long double lgammal(long double x)
{
	long double r = __lgammal_r(x, &__signgam);
	return x - x != 0 ? r : __math_l(r);
}

double lgamma(double x)
{
	return __math_d(__lgammal_r(x, &__signgam));
}

float lgammaf(float x)
{
	return __math_f(__lgammal_r(x, &__signgam));
}
