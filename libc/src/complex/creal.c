#include "complex_impl.h"

long double creall(long double _Complex z) { return __cre(z); }
double creal(double _Complex z) { return __cre(z); }
float crealf(float _Complex z) { return __cre(z); }

long double cimagl(long double _Complex z) { return __cim(z); }
double cimag(double _Complex z) { return __cim(z); }
float cimagf(float _Complex z) { return __cim(z); }

long double _Complex conjl(long double _Complex z) { return __cpack(__cre(z), -__cim(z)); }
__CWRAP(conj)

/* the projection onto the Riemann sphere */
long double _Complex cprojl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z);
	if ((__ld_nonfinite(x) && x == x) || (__ld_nonfinite(y) && y == y))   /* an infinite part */
		return __cpack(HUGE_VALL, copysignl(0, y));
	return z;
}
__CWRAP(cproj)
