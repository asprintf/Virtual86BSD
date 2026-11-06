#include "complex_impl.h"

long double _Complex cexpl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), e, c, s;
	if (y == 0)
		return __cpack(expl(x), y);
	if (__ld_nonfinite(y)) {                       /* y infinite or NaN */
		if (__ld_nonfinite(x) && x == x)        /* x infinite */
			return x > 0 ? __cpack(x, y - y) : __cpack(0, 0);
		return __cpack(y - y, y - y);
	}
	if (x != x)
		return __cpack(x, x);
	c = cosl(y);
	s = sinl(y);
	if (__ld_nonfinite(x))                          /* +-inf: exp(x) cis y */
		return x > 0 ? __cpack(x * c, x * s) : __cpack(0 * c, 0 * s);
	if (x > 11356) {                         /* e^x overflows, maybe not the parts */
		e = expl(x / 2);
		return __cpack(e * c * e, e * s * e);
	}
	e = expl(x);
	return __cpack(e * c, e * s);
}
__CWRAP(cexp)
