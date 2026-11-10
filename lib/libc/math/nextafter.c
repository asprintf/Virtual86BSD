#include "libm.h"
#include <fenv.h>

/* errno and the exceptions for a result r that is infinite (from a
 * finite x) or subnormal or zero */
static void range(int inf, int tiny)
{
	if (inf) {
		errno = ERANGE;
		feraiseexcept(FE_OVERFLOW | FE_INEXACT);
	} else if (tiny) {
		errno = ERANGE;
		feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
	}
}

/* the next double after x towards y (y compared in long double) */
static double next(double x, long double y)
{
	union __dbits u = {x};
	if (x != x || y != y)
		return x + y;
	if (x == y)
		return y;
	if (x == 0) {   /* no errno for this underflow, as glibc */
		u.i = (y < 0 ? 1ULL << 63 : 0) | 1;
		feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
		return u.f;
	}
	else if ((x < y) == (x > 0))
		u.i++;
	else
		u.i--;
	range(u.f - u.f != 0, u.f < DBL_MIN && u.f > -DBL_MIN);
	return u.f;
}

static float nextf(float x, long double y)
{
	union __fbits u = {x};
	if (x != x || y != y)
		return x + y;
	if (x == y)
		return y;
	if (x == 0) {
		u.i = (y < 0 ? 0x80000000 : 0) | 1;
		feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
		return u.f;
	}
	else if ((x < y) == (x > 0))
		u.i++;
	else
		u.i--;
	range(u.f - u.f != 0, u.f < FLT_MIN && u.f > -FLT_MIN);
	return u.f;
}

double nextafter(double x, double y) { return next(x, y); }
double nexttoward(double x, long double y) { return next(x, y); }
float nextafterf(float x, float y) { return nextf(x, y); }
float nexttowardf(float x, long double y) { return nextf(x, y); }

long double nextafterl(long double x, long double y)
{
	union __ldbits u = {x};
	int e;
	if (x != x || y != y)
		return x + y;
	if (x == y)
		return y;
	if (x == 0) {
		u.i.m = 1;
		u.i.se = y < 0 ? 0x8000 : 0;
		feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
		return u.f;
	} else if ((x < y) == (x > 0)) {        /* away from zero */
		u.i.m++;
		if (u.i.m == 0) {               /* 0xffff... + 1 */
			u.i.m = 1ULL << 63;
			u.i.se++;
		} else if (u.i.m == 1ULL << 63 && !(u.i.se & 0x7fff))
			u.i.se++;               /* subnormal to normal */
	} else {                                /* towards zero */
		e = u.i.se & 0x7fff;
		if (u.i.m == 1ULL << 63 && e > 0) {
			u.i.m = e > 1 ? -1ULL : (1ULL << 63) - 1;
			u.i.se--;
		} else
			u.i.m--;
	}
	range(u.f - u.f != 0, u.f < LDBL_MIN && u.f > -LDBL_MIN);
	return u.f;
}

long double nexttowardl(long double x, long double y) { return nextafterl(x, y); }
