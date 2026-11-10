/* erf and erfc in long double.
 *  |x| < 2: erf x = 2/sqrt(pi) e^(-x^2) sum 2^n x^(2n+1) / (1 3 5 ... (2n+1))
 *           (positive terms, no cancellation)
 *  x >= 0.5: erfc x = e^(-x^2)/sqrt(pi) / (x + (1/2)/(x + 1/(x + (3/2)/(x + ...))))
 *           (Laplace's continued fraction, evaluated backwards), with
 *           e^(-x^2) from x^2 = h + l exactly
 */
#include "libm.h"

#define SQRTPI_INV 0.564189583547756286948079451560772586L

static long double erf_series(long double x)     /* x >= 0 */
{
	long double x2 = x * x, t = x, s = x;
	int n;
	for (n = 1; n < 200; n++) {
		t *= 2 * x2 / (2 * n + 1);
		s += t;
		if (t < s * 0x1p-66L)
			break;
	}
	return 2 * SQRTPI_INV * __expl(-x2) * s;
}

static long double erfc_cf(long double x)
{
	long double f = x, h, l, e;
	int n = 20 + (int)(350 / (x * x));
	for (; n >= 1; n--)
		f = x + (n * 0.5L) / f;
	__two_prod(x, x, &h, &l);
	if (h > 11400)
		return 0;
	e = __expl(-h);
	return e * (1 - l) * SQRTPI_INV / f;
}

long double erfl(long double x)
{
	long double ax = __ld_abs(x), r;
	if (x != x)
		return x + x;
	if (ax < 0x1p-32L)
		return __math_l(2 * SQRTPI_INV * x);
	if (ax < 2)
		r = erf_series(ax);
	else
		r = ax > 7 ? 1 : 1 - erfc_cf(ax);
	return x < 0 ? -r : r;
}

long double erfcl(long double x)
{
	long double r;
	if (x != x)
		return x + x;
	if (x < 0.5L)
		return x < -7 ? 2 : 1 - erfl(x);
	if (__ld_nonfinite(x))
		return 0;
	r = erfc_cf(x);
	if (r == 0)
		return __math_uflow(0);
	return r;
}

__MATH_WRAP1(erf)
__MATH_WRAP1(erfc)
