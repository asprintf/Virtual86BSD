/* trunc, floor, ceil and round: exact, so the double and float ones are
 * the long double ones */
#include "libm.h"
#include <fenv.h>

long double truncl(long double x)
{
	union __ldbits u = {x};
	int e = (u.i.se & 0x7fff) - 16383;
	if (e >= 63)
		return x + 0;           /* integer, infinite or NaN */
	if (e < 0)
		return u.i.se >> 15 ? -0.0L : 0.0L;
	u.i.m &= ~((1ULL << (63 - e)) - 1);
	return u.f;
}

long double floorl(long double x)
{
	long double t = truncl(x);
	return t > x ? t - 1 : t;
}

long double ceill(long double x)
{
	long double t = truncl(x);
	return t < x ? t + 1 : t;
}

long double roundl(long double x)
{
	if (__ld_nonfinite(x))
		return x + 0;           /* no inf - inf */
	long double t = truncl(x);
	if (__ld_abs(x - t) >= 0.5L)
		t += x < 0 ? -1 : 1;
	return t;
}

double trunc(double x) { return truncl(x); }
float truncf(float x) { return truncl(x); }
double floor(double x) { return floorl(x); }
float floorf(float x) { return floorl(x); }
double ceil(double x) { return ceill(x); }
float ceilf(float x) { return ceill(x); }
double round(double x) { return roundl(x); }
float roundf(float x) { return roundl(x); }

/* lround, llround: out of range, EDOM, FE_INVALID and the smallest value */
long lroundl(long double x)
{
	long double t = roundl(x);
	if (!(t >= -0x1p63L && t < 0x1p63L)) {
		errno = EDOM;             /* a domain error (POSIX): NaN, infinite, too large */
		feraiseexcept(FE_INVALID);
		return (long)(-0x7fffffffffffffffLL - 1);
	}
	return (long)t;
}

long lround(double x) { return lroundl(x); }
long lroundf(float x) { return lroundl(x); }
long long llroundl(long double x) { return lroundl(x); }
long long llround(double x) { return lroundl(x); }
long long llroundf(float x) { return lroundl(x); }
