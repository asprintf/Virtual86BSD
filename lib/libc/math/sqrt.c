#include "libm.h"

/* the hardware rounds correctly in each format */
double sqrt(double x)
{
	if (x < 0)
		errno = EDOM;
	return __sse_sqrt(x);
}

float sqrtf(float x)
{
	if (x < 0)
		errno = EDOM;
	return __sse_sqrtf(x);
}

long double sqrtl(long double x)
{
	if (x < 0)
		errno = EDOM;
	return __x87_sqrt(x);
}
