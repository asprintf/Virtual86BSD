/* catanh, and catan z = -i atanh(iz) (Kahan):
 * atanh z = log1p(4x / ((1-x)^2 + y^2)) / 4 + i atan2(2y, (1-x)(1+x) - y^2) / 2 */
#include "complex_impl.h"

#define INF(v) ((v) - (v) != 0 && (v) == (v))

long double _Complex catanhl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), ax = fabsl(x), ay = fabsl(y), r;
	if (INF(x) || INF(y))
		return __cpack(copysignl(0, x), y == y ? copysignl(__PIO2, y) : y);
	if (x != x || y != y) {
		if (x == 0)
			return __cpack(x, y);
		return __cpack(x + y, x + y);
	}
	if (ax > 0x1p32L || ay > 0x1p32L) {      /* 1/z + i pi/2 sign y */
		r = hypotl(x, y);
		return __cpack(x / r / r, copysignl(__PIO2, y));
	}
	if (ax == 1 && y == 0)
		return __cpack(x / 0.0L, y);     /* pole */
	/* t = 4x / ((1-x)^2 + y^2); where |t| is large, 1 + t is better
	 * computed as ((1+x)^2 + y^2) / ((1-x)^2 + y^2): near x = +-1 the
	 * factor that vanishes is exact */
	{
		long double d = (1 - x) * (1 - x) + y * y, t = 4 * x / d, re;
		if (fabsl(t) > 0.5L)
			re = logl(((1 + x) * (1 + x) + y * y) / d) / 4;
		else
			re = log1pl(t) / 4;
		return __cpack(re, atan2l(2 * y, (1 - x) * (1 + x) - y * y) / 2);
	}
}

long double _Complex catanl(long double _Complex z)
{
	long double _Complex w = catanhl(__cpack(-__cim(z), __cre(z)));
	return __cpack(__cim(w), -__cre(w));
}

__CWRAP(catanh)
__CWRAP(catan)
