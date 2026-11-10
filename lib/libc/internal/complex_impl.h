/* Internal: the complex functions.  They compute in long double on the
 * real and imaginary parts (no complex arithmetic, which would need the
 * compiler's run-time support); the float and double functions are the
 * long double ones, each part rounded once. */
#ifndef _COMPLEX_IMPL_H
#define _COMPLEX_IMPL_H
#include <complex.h>
#include "libm.h"

typedef long double _Complex __ldc;
union __ldcu { __ldc z; long double a[2]; };

static inline __ldc __cpack(long double re, long double im)
{
	union __ldcu u;
	u.a[0] = re;
	u.a[1] = im;
	return u.z;
}

static inline long double __cre(__ldc z) { union __ldcu u; u.z = z; return u.a[0]; }
static inline long double __cim(__ldc z) { union __ldcu u; u.z = z; return u.a[1]; }

/* csqrt of x + iy, as parts */
void __csqrt_parts(long double x, long double y, long double *re, long double *im);

#define __CWRAP(N) \
	double _Complex N(double _Complex z) { return N##l(z); } \
	float _Complex N##f(float _Complex z) { return N##l(z); }

#endif
