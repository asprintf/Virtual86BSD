/* rint, nearbyint, lrint, llrint: in the current rounding mode, which
 * fesetround() keeps the same for the x87 unit and SSE */
#include "libm.h"
#include <fenv.h>

long double rintl(long double x) { return __x87_rint(x); }
double rint(double x) { return __x87_rint(x); }
float rintf(float x) { return __x87_rint(x); }

long double nearbyintl(long double x)
{
	int e = fetestexcept(FE_INEXACT);
	long double r = __x87_rint(x);
	if (!e)
		feclearexcept(FE_INEXACT);
	return r;
}

double nearbyint(double x) { return nearbyintl(x); }
float nearbyintf(float x) { return nearbyintl(x); }

long lrintl(long double x)
{
	long double t = __x87_rint(x);
	if (!(t >= -0x1p63L && t < 0x1p63L)) {
		feraiseexcept(FE_INVALID);
		return (long)(-0x7fffffffffffffffLL - 1);
	}
	return (long)t;
}

long lrint(double x) { return lrintl(x); }
long lrintf(float x) { return lrintl(x); }
long long llrintl(long double x) { return lrintl(x); }
long long llrint(double x) { return lrintl(x); }
long long llrintf(float x) { return lrintl(x); }
