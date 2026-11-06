#include "complex_impl.h"

/* log |x + iy|, accurately also near |z| = 1 */
static long double loghypot(long double x, long double y)
{
	long double ax = fabsl(x), ay = fabsl(y), t, h1, l1, h2, l2;
	if ((__ld_nonfinite(ax) && ax == ax) || (__ld_nonfinite(ay) && ay == ay))
		return HUGE_VALL;
	if (ax != ax || ay != ay)
		return ax + ay;
	if (ax < ay) {
		t = ax; ax = ay; ay = t;
	}
	if (ax == 0)
		return -1.0L / ax;              /* pole: FE_DIVBYZERO */
	if (ax > 0.5L && ax < 1.5L && ay < 0x1p-1L) {
		/* (ax^2 - 1) + ay^2 exactly enough, then log1p */
		__two_prod(ax, ax, &h1, &l1);
		__two_prod(ay, ay, &h2, &l2);
		t = h1 - 1;                     /* exact */
		t = ((t + h2) + l1) + l2;
		if (fabsl(t) < 0.5L)
			return log1pl(t) / 2;
	}
	return logl(hypotl(ax, ay));
}

long double _Complex clogl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z);
	return __cpack(loghypot(x, y), atan2l(y, x));
}
__CWRAP(clog)

/* z^w = e^(w log z) */
long double _Complex cpowl(long double _Complex z, long double _Complex w)
{
	long double _Complex l = clogl(z);
	long double a = __cre(l), b = __cim(l), c = __cre(w), d = __cim(w);
	return cexpl(__cpack(a * c - b * d, a * d + b * c));
}

double _Complex cpow(double _Complex z, double _Complex w) { return cpowl(z, w); }
float _Complex cpowf(float _Complex z, float _Complex w) { return cpowl(z, w); }
