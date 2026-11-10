#ifndef _TGMATH_H
#define _TGMATH_H
#include <math.h>
#include <complex.h>

/* Type-generic macros (C17 7.25) by C11 _Generic.  The type is that of
 * the arguments with integer types counted as double: __TG_T adds to
 * the sum of the arguments a zero of their real type (float, double or
 * long double), which also keeps complex arguments complex. */
#define __TG_Z(x) _Generic((x), float: 0.0f, float _Complex: 0.0f, long double: 0.0L, \
	long double _Complex: 0.0L, default: 0.0)
#define __TG_T1(x)       ((x) + __TG_Z(x))
#define __TG_T2(x, y)    ((x) + (y) + __TG_Z(x) + __TG_Z(y))
#define __TG_T3(x, y, z) ((x) + (y) + (z) + __TG_Z(x) + __TG_Z(y) + __TG_Z(z))

/* real functions */
#define __TG_R(e, N) _Generic(e, float: N##f, long double: N##l, default: N)
/* real and complex functions */
#define __TG_RC(e, N) _Generic(e, float: N##f, long double: N##l, default: N, \
	float _Complex: c##N##f, double _Complex: c##N, long double _Complex: c##N##l)
/* complex functions (real arguments count as complex) */
#define __TG_C(e, N) _Generic(e, float: N##f, float _Complex: N##f, \
	long double: N##l, long double _Complex: N##l, default: N)

#define acos(x)  __TG_RC(__TG_T1(x), acos)(x)
#define asin(x)  __TG_RC(__TG_T1(x), asin)(x)
#define atan(x)  __TG_RC(__TG_T1(x), atan)(x)
#define acosh(x) __TG_RC(__TG_T1(x), acosh)(x)
#define asinh(x) __TG_RC(__TG_T1(x), asinh)(x)
#define atanh(x) __TG_RC(__TG_T1(x), atanh)(x)
#define cos(x)   __TG_RC(__TG_T1(x), cos)(x)
#define sin(x)   __TG_RC(__TG_T1(x), sin)(x)
#define tan(x)   __TG_RC(__TG_T1(x), tan)(x)
#define cosh(x)  __TG_RC(__TG_T1(x), cosh)(x)
#define sinh(x)  __TG_RC(__TG_T1(x), sinh)(x)
#define tanh(x)  __TG_RC(__TG_T1(x), tanh)(x)
#define exp(x)   __TG_RC(__TG_T1(x), exp)(x)
#define log(x)   __TG_RC(__TG_T1(x), log)(x)
#define pow(x, y) __TG_RC(__TG_T2(x, y), pow)(x, y)
#define sqrt(x)  __TG_RC(__TG_T1(x), sqrt)(x)
#define fabs(x)  _Generic(__TG_T1(x), float: fabsf, long double: fabsl, default: fabs, \
	float _Complex: cabsf, double _Complex: cabs, long double _Complex: cabsl)(x)

#define atan2(x, y)     __TG_R(__TG_T2(x, y), atan2)(x, y)
#define cbrt(x)         __TG_R(__TG_T1(x), cbrt)(x)
#define ceil(x)         __TG_R(__TG_T1(x), ceil)(x)
#define copysign(x, y)  __TG_R(__TG_T2(x, y), copysign)(x, y)
#define erf(x)          __TG_R(__TG_T1(x), erf)(x)
#define erfc(x)         __TG_R(__TG_T1(x), erfc)(x)
#define exp2(x)         __TG_R(__TG_T1(x), exp2)(x)
#define expm1(x)        __TG_R(__TG_T1(x), expm1)(x)
#define fdim(x, y)      __TG_R(__TG_T2(x, y), fdim)(x, y)
#define floor(x)        __TG_R(__TG_T1(x), floor)(x)
#define fma(x, y, z)    __TG_R(__TG_T3(x, y, z), fma)(x, y, z)
#define fmax(x, y)      __TG_R(__TG_T2(x, y), fmax)(x, y)
#define fmin(x, y)      __TG_R(__TG_T2(x, y), fmin)(x, y)
#define fmod(x, y)      __TG_R(__TG_T2(x, y), fmod)(x, y)
#define frexp(x, e)     __TG_R(__TG_T1(x), frexp)(x, e)
#define hypot(x, y)     __TG_R(__TG_T2(x, y), hypot)(x, y)
#define ilogb(x)        __TG_R(__TG_T1(x), ilogb)(x)
#define ldexp(x, e)     __TG_R(__TG_T1(x), ldexp)(x, e)
#define lgamma(x)       __TG_R(__TG_T1(x), lgamma)(x)
#define llrint(x)       __TG_R(__TG_T1(x), llrint)(x)
#define llround(x)      __TG_R(__TG_T1(x), llround)(x)
#define log10(x)        __TG_R(__TG_T1(x), log10)(x)
#define log1p(x)        __TG_R(__TG_T1(x), log1p)(x)
#define log2(x)         __TG_R(__TG_T1(x), log2)(x)
#define logb(x)         __TG_R(__TG_T1(x), logb)(x)
#define lrint(x)        __TG_R(__TG_T1(x), lrint)(x)
#define lround(x)       __TG_R(__TG_T1(x), lround)(x)
#define nearbyint(x)    __TG_R(__TG_T1(x), nearbyint)(x)
#define nextafter(x, y) __TG_R(__TG_T2(x, y), nextafter)(x, y)
#define nexttoward(x, y) __TG_R(__TG_T1(x), nexttoward)(x, y)
#define remainder(x, y) __TG_R(__TG_T2(x, y), remainder)(x, y)
#define remquo(x, y, q) __TG_R(__TG_T2(x, y), remquo)(x, y, q)
#define rint(x)         __TG_R(__TG_T1(x), rint)(x)
#define round(x)        __TG_R(__TG_T1(x), round)(x)
#define scalbn(x, e)    __TG_R(__TG_T1(x), scalbn)(x, e)
#define scalbln(x, e)   __TG_R(__TG_T1(x), scalbln)(x, e)
#define tgamma(x)       __TG_R(__TG_T1(x), tgamma)(x)
#define trunc(x)        __TG_R(__TG_T1(x), trunc)(x)

#define carg(x)  __TG_C(__TG_T1(x), carg)(x)
#define cimag(x) __TG_C(__TG_T1(x), cimag)(x)
#define conj(x)  __TG_C(__TG_T1(x), conj)(x)
#define cproj(x) __TG_C(__TG_T1(x), cproj)(x)
#define creal(x) __TG_C(__TG_T1(x), creal)(x)

#endif
