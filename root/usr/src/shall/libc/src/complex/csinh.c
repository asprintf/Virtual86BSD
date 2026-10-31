/* csinh, ccosh, ctanh, and csin, ccos, ctan by sin z = -i sinh(iz),
 * cos z = cosh(iz), tan z = -i tanh(iz); the special values of C Annex G */
#include "complex_impl.h"

#define INF(v) ((v) - (v) != 0 && (v) == (v))

long double _Complex csinhl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z);
	if (y == 0)
		return __cpack(sinhl(x), y);
	if (y - y != 0) {                       /* y infinite or NaN */
		if (x == 0)
			return __cpack(x, y - y);
		if (INF(x))
			return __cpack(fabsl(x), y - y);     /* +-inf: sign unspecified */
		return __cpack(y - y + x, y - y + x);
	}
	if (x != x)
		return __cpack(x, x);
	if (INF(x))
		return __cpack(x * cosl(y), fabsl(x) * sinl(y));
	return __cpack(sinhl(x) * cosl(y), coshl(x) * sinl(y));
}

long double _Complex ccoshl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z);
	if (y == 0)
		return __cpack(coshl(x), x == x ? copysignl(0, x) * y : y);
	if (y - y != 0) {
		if (x == 0)
			return __cpack(y - y, x * copysignl(0, y));
		if (INF(x))
			return __cpack(fabsl(x), y - y);
		return __cpack(y - y + x, y - y + x);
	}
	if (x != x)
		return __cpack(x, x);
	if (INF(x))
		return __cpack(fabsl(x) * cosl(y), x * sinl(y));
	return __cpack(coshl(x) * cosl(y), sinhl(x) * sinl(y));
}

/* Kahan: t = tan y, b = 1 + t^2, s = sinh x, r = sqrt(1 + s^2):
 * tanh z = (b r s + i t) / (1 + b s^2) */
long double _Complex ctanhl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), t, b, s, r, d;
	if (INF(x))
		return __cpack(copysignl(1, x), copysignl(0, y - y == 0 ? sinl(2 * y) : y));
	if (x != x)
		return __cpack(x, y == 0 ? y : x);
	if (y - y != 0)
		return __cpack(x == 0 ? x : y - y, y - y);
	if (fabsl(x) > 23) {
		long double e = expl(-2 * fabsl(x));
		return __cpack(copysignl(1, x), 4 * sinl(y) * cosl(y) * e);
	}
	t = tanl(y);
	b = 1 + t * t;
	s = sinhl(x);
	r = sqrtl(1 + s * s);
	d = 1 + b * s * s;
	return __cpack(b * r * s / d, t / d);
}

long double _Complex csinl(long double _Complex z)
{
	long double _Complex w = csinhl(__cpack(-__cim(z), __cre(z)));
	return __cpack(__cim(w), -__cre(w));
}

long double _Complex ccosl(long double _Complex z)
{
	return ccoshl(__cpack(-__cim(z), __cre(z)));
}

long double _Complex ctanl(long double _Complex z)
{
	long double _Complex w = ctanhl(__cpack(-__cim(z), __cre(z)));
	return __cpack(__cim(w), -__cre(w));
}

__CWRAP(csinh)
__CWRAP(ccosh)
__CWRAP(ctanh)
__CWRAP(csin)
__CWRAP(ccos)
__CWRAP(ctan)
