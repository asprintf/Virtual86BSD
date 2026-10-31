/* casinh, cacosh, and casin, cacos from them, after Kahan ("Branch cuts
 * for complex elementary functions", 1987); for large |z| (and infinite
 * parts) the asymptotic log(2z). */
#include "complex_impl.h"

#define BIG 0x1p33L

/* conj(a) * b, imaginary part, and real part */
static long double imconjmul(long double ar, long double ai, long double br, long double bi)
{
	return ar * bi - ai * br;
}

static long double reconjmul(long double ar, long double ai, long double br, long double bi)
{
	return ar * br + ai * bi;
}

long double _Complex casinhl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), ax = fabsl(x), r1, i1, r2, i2, xi, eta;
	if (y == 0)
		return __cpack(asinhl(x), y);
	if (!(ax < BIG && fabsl(y) < BIG) && (ax == ax || fabsl(y) - fabsl(y) != 0)) {
		/* log(2|z|) + i arg, odd in x */
		long double h = hypotl(x, y);
		return __cpack(copysignl(logl(h) + __LN2, x), atan2l(y, ax));
	}
	if (x != x || y != y)
		return __cpack(x + y, x + y);
	/* asinh z = -i asin(iz); asin w = atan2(Re w, Re(sqrt(1-w) sqrt(1+w)))
	 * + i asinh(Im(conj(sqrt(1-w)) sqrt(1+w))), w = -y + ix */
	__csqrt_parts(1 + y, -x, &r1, &i1);    /* sqrt(1 - w) */
	__csqrt_parts(1 - y, x, &r2, &i2);     /* sqrt(1 + w) */
	xi = atan2l(-y, r1 * r2 - i1 * i2);
	eta = asinhl(imconjmul(r1, i1, r2, i2));
	return __cpack(eta, -xi);
}

long double _Complex cacoshl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), r1, i1, r2, i2;
	if (!(fabsl(x) < BIG && fabsl(y) < BIG) && (x == x || fabsl(y) - fabsl(y) != 0)
	    && (y == y || fabsl(x) - fabsl(x) != 0))
		return __cpack(logl(hypotl(x, y)) + __LN2, atan2l(y, x));
	if (x != x || y != y) {
		if (x == 0)     /* NaN + i pi/2 */
			return __cpack(y, __PIO2);
		return __cpack(x + y, x + y);
	}
	/* acosh z = asinh(Re(conj(sqrt(z-1)) sqrt(z+1)))
	 *         + 2i atan2(Im sqrt(z-1), Re sqrt(z+1)) */
	__csqrt_parts(x - 1, y, &r1, &i1);
	__csqrt_parts(x + 1, y, &r2, &i2);
	return __cpack(asinhl(reconjmul(r1, i1, r2, i2)), 2 * atan2l(i1, r2));
}

/* asin z = -i asinh(iz) */
long double _Complex casinl(long double _Complex z)
{
	long double _Complex w = casinhl(__cpack(-__cim(z), __cre(z)));
	return __cpack(__cim(w), -__cre(w));
}

/* acos z: acosh z = i acos z (Im z >= 0), -i acos z (Im z < 0) */
long double _Complex cacosl(long double _Complex z)
{
	long double _Complex w = cacoshl(z);
	long double u = __cre(w), v = __cim(w);
	return __cpack(fabsl(v), signbit(__cim(z)) ? u : -u);
}

__CWRAP(casinh)
__CWRAP(cacosh)
__CWRAP(casin)
__CWRAP(cacos)
