/* Rounding to the result type, and the error results, with errno. */
#include "libm.h"

double __math_d(long double y)
{
	double r = (double)y;
	if (r - r != 0) {                       /* infinite or NaN */
		if (y - y == 0)                 /* finite y: overflow */
			errno = ERANGE;
	} else if (r == 0 && y != 0)
		errno = ERANGE;                 /* underflow to zero */
	return r;
}

float __math_f(long double y)
{
	float r = (float)y;
	if (r - r != 0) {
		if (y - y == 0)
			errno = ERANGE;
	} else if (r == 0 && y != 0)
		errno = ERANGE;
	return r;
}

/* For long double results the computation itself overflows; y is the
 * result for a finite argument.  (As glibc, errno is set for underflow
 * only when the result is zero: the callers check for that.) */
long double __math_l(long double y)
{
	if (y - y != 0 && y == y)
		errno = ERANGE;
	return y;
}

long double __math_invalid(long double x)
{
	if (x == x)
		errno = EDOM;
	return (x - x) / (x - x);
}

long double __math_divzero(int neg)
{
	errno = ERANGE;
	return (neg ? -1.0L : 1.0L) / 0.0L;
}

long double __math_oflow(int neg)
{
	volatile long double h = 0x1p16000L;
	errno = ERANGE;
	return (neg ? -h : h) * h;
}

long double __math_uflow(int neg)
{
	volatile long double t = 0x1p-16000L;
	errno = ERANGE;
	return (neg ? -t : t) * t;
}
