#include "libm.h"

double fabs(double x) { union __dbits u = {x}; u.i &= -1ULL >> 1; return u.f; }
float fabsf(float x) { union __fbits u = {x}; u.i &= 0x7fffffff; return u.f; }
long double fabsl(long double x) { return __ld_abs(x); }

double copysign(double x, double y)
{
	union __dbits u = {x}, v = {y};
	u.i = (u.i & -1ULL >> 1) | (v.i & 1ULL << 63);
	return u.f;
}

float copysignf(float x, float y)
{
	union __fbits u = {x}, v = {y};
	u.i = (u.i & 0x7fffffff) | (v.i & 0x80000000);
	return u.f;
}

long double copysignl(long double x, long double y)
{
	union __ldbits u = {x}, v = {y};
	u.i.se = (u.i.se & 0x7fff) | (v.i.se & 0x8000);
	return u.f;
}
