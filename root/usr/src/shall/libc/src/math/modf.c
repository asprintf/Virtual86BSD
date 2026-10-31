#include "libm.h"

long double modfl(long double x, long double *ip)
{
	long double t = truncl(x);
	*ip = t;
	if (x != x)
		return x + x;
	if (x - x != 0)
		return x < 0 ? -0.0L : 0.0L;
	return copysignl(x - t, x);
}

double modf(double x, double *ip)
{
	long double t, r = modfl(x, &t);
	*ip = t;
	return r;
}

float modff(float x, float *ip)
{
	long double t, r = modfl(x, &t);
	*ip = t;
	return r;
}
