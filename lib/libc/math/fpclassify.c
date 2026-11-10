/* The classification and comparison functions behind the macros of
 * <math.h> (used by compilers without the GCC built-ins).  Comparisons
 * of quiet NaNs raise no exception. */
#include "libm.h"

int __fpclassify(double x)
{
	union __dbits u = {x};
	int e = u.i >> 52 & 0x7ff;
	if (e == 0)
		return u.i << 1 ? FP_SUBNORMAL : FP_ZERO;
	if (e == 0x7ff)
		return u.i << 12 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

int __fpclassifyf(float x)
{
	union __fbits u = {x};
	int e = u.i >> 23 & 0xff;
	if (e == 0)
		return u.i << 1 ? FP_SUBNORMAL : FP_ZERO;
	if (e == 0xff)
		return u.i << 9 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

/* the x87 format has an explicit integer bit: encodings with the wrong
 * one (unnormals, pseudo-NaNs, pseudo-infinities) count as NaN */
int __fpclassifyl(long double x)
{
	union __ldbits u = {x};
	int e = u.i.se & 0x7fff;
	if (e == 0)
		return u.i.m ? FP_SUBNORMAL : FP_ZERO;
	if (!(u.i.m >> 63))
		return FP_NAN;
	if (e == 0x7fff)
		return u.i.m << 1 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

int __signbit(double x) { union __dbits u = {x}; return (int)(u.i >> 63); }
int __signbitf(float x) { union __fbits u = {x}; return (int)(u.i >> 31); }
int __signbitl(long double x) { union __ldbits u = {x}; return u.i.se >> 15; }

#define FUNCS(S, T) \
int __isfinite##S(T x) { return __fpclassify##S(x) >= FP_ZERO; } \
int __isnan##S(T x) { return __fpclassify##S(x) == FP_NAN; } \
int __isnormal##S(T x) { return __fpclassify##S(x) == FP_NORMAL; } \
int __isinf##S(T x) { return __fpclassify##S(x) != FP_INFINITE ? 0 : __signbit##S(x) ? -1 : 1; } \
int __isunordered##S(T x, T y) { return __isnan##S(x) || __isnan##S(y); } \
int __isgreater##S(T x, T y) { return !__isunordered##S(x, y) && x > y; } \
int __isgreaterequal##S(T x, T y) { return !__isunordered##S(x, y) && x >= y; } \
int __isless##S(T x, T y) { return !__isunordered##S(x, y) && x < y; } \
int __islessequal##S(T x, T y) { return !__isunordered##S(x, y) && x <= y; } \
int __islessgreater##S(T x, T y) { return !__isunordered##S(x, y) && x != y; }

FUNCS(, double)
FUNCS(f, float)
FUNCS(l, long double)
