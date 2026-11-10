/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MATH_H
#define _MATH_H
#include <bits/features.h>

/* FLT_EVAL_METHOD is 0 on x86-64 (SSE arithmetic). */
typedef float float_t;
typedef double double_t;

/* constants, raising no floating-point exception (GCC, Clang and c17
   have these built-ins) */
#if defined(__GNUC__) || defined(__c17__)
# define HUGE_VAL  (__builtin_huge_val())
# define HUGE_VALF (__builtin_huge_valf())
# define HUGE_VALL (__builtin_huge_vall())
# define INFINITY  (__builtin_inff())
# define NAN       (__builtin_nanf(""))
#else
# define HUGE_VAL  (1.0 / 0.0)
# define HUGE_VALF (1.0f / 0.0f)
# define HUGE_VALL (1.0L / 0.0L)
# define INFINITY  (1.0f / 0.0f)
# define NAN       (-(0.0f / 0.0f))
#endif

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#define FP_ILOGB0   (-2147483647 - 1)
#define FP_ILOGBNAN (-2147483647 - 1)

#define MATH_ERRNO       1
#define MATH_ERREXCEPT   2
#define math_errhandling 3

#if __XSI_VISIBLE
#define M_E          2.7182818284590452354
#define M_EGAMMA     0.57721566490153286061
#define M_LOG2E      1.4426950408889634074
#define M_LOG10E     0.43429448190325182765
#define M_LN2        0.69314718055994530942
#define M_LN10       2.30258509299404568402
#define M_PHI        1.61803398874989484820
#define M_PI         3.14159265358979323846
#define M_PI_2       1.57079632679489661923
#define M_PI_4       0.78539816339744830962
#define M_1_PI       0.31830988618379067154
#define M_1_SQRTPI   0.56418958354775628695
#define M_2_PI       0.63661977236758134308
#define M_2_SQRTPI   1.12837916709551257390
#define M_SQRT2      1.41421356237309504880
#define M_SQRT3      1.73205080756887729353
#define M_SQRT1_2    0.70710678118654752440
#define M_SQRT1_3    0.57735026918962576451
#define M_El         2.718281828459045235360287471352662498L
#define M_EGAMMAl    0.577215664901532860606512090082402431L
#define M_LOG2El     1.442695040888963407359924681001892137L
#define M_LOG10El    0.434294481903251827651128918916605082L
#define M_LN2l       0.693147180559945309417232121458176568L
#define M_LN10l      2.302585092994045684017991454684364208L
#define M_PHIl       1.618033988749894848204586834365638118L
#define M_PIl        3.141592653589793238462643383279502884L
#define M_PI_2l      1.570796326794896619231321691639751442L
#define M_PI_4l      0.785398163397448309615660845819875721L
#define M_1_PIl      0.318309886183790671537767526745028724L
#define M_1_SQRTPIl  0.564189583547756286948079451560772586L
#define M_2_PIl      0.636619772367581343075535053490057448L
#define M_2_SQRTPIl  1.128379167095512573896158903121545172L
#define M_SQRT2l     1.414213562373095048801688724209698079L
#define M_SQRT3l     1.732050807568877293527446341505872367L
#define M_SQRT1_2l   0.707106781186547524400844362104849039L
#define M_SQRT1_3l   0.577350269189625764509148780501957456L
#endif

__BEGIN_DECLS
int __fpclassify(double);
int __fpclassifyf(float);
int __fpclassifyl(long double);
int __signbit(double);
int __signbitf(float);
int __signbitl(long double);
int __isunordered(double, double);
int __isunorderedf(float, float);
int __isunorderedl(long double, long double);
__END_DECLS

/* Classification and comparison.  GCC has built-ins for all of them;
 * otherwise C11 _Generic selects a function by the argument type
 * (integer arguments count as double), so each argument is evaluated
 * once and the comparisons raise no exception for quiet NaNs. */
#if defined(__GNUC__) && !defined(__c17__)
# define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
# define isfinite(x)   __builtin_isfinite(x)
# define isinf(x)      __builtin_isinf_sign(x)
# define isnan(x)      __builtin_isnan(x)
# define isnormal(x)   __builtin_isnormal(x)
# define signbit(x)    __builtin_signbit(x)
# define isgreater(x, y)      __builtin_isgreater(x, y)
# define isgreaterequal(x, y) __builtin_isgreaterequal(x, y)
# define isless(x, y)         __builtin_isless(x, y)
# define islessequal(x, y)    __builtin_islessequal(x, y)
# define islessgreater(x, y)  __builtin_islessgreater(x, y)
# define isunordered(x, y)    __builtin_isunordered(x, y)
#else
__BEGIN_DECLS
int __isfinite(double), __isfinitef(float), __isfinitel(long double);
int __isinf(double), __isinff(float), __isinfl(long double);
int __isnan(double), __isnanf(float), __isnanl(long double);
int __isnormal(double), __isnormalf(float), __isnormall(long double);
int __isgreater(double, double), __isgreaterf(float, float), __isgreaterl(long double, long double);
int __isgreaterequal(double, double), __isgreaterequalf(float, float),
    __isgreaterequall(long double, long double);
int __isless(double, double), __islessf(float, float), __islessl(long double, long double);
int __islessequal(double, double), __islessequalf(float, float), __islessequall(long double, long double);
int __islessgreater(double, double), __islessgreaterf(float, float),
    __islessgreaterl(long double, long double);
__END_DECLS
# define __MATH_TG1(N, x) _Generic((x), float: N##f, long double: N##l, default: N)(x)
# define __MATH_TG2(N, x, y) _Generic((x) + (y), float: N##f, long double: N##l, default: N)(x, y)
# define fpclassify(x) __MATH_TG1(__fpclassify, x)
# define isfinite(x)   __MATH_TG1(__isfinite, x)
# define isinf(x)      __MATH_TG1(__isinf, x)
# define isnan(x)      __MATH_TG1(__isnan, x)
# define isnormal(x)   __MATH_TG1(__isnormal, x)
# define signbit(x)    __MATH_TG1(__signbit, x)
# define isgreater(x, y)      __MATH_TG2(__isgreater, x, y)
# define isgreaterequal(x, y) __MATH_TG2(__isgreaterequal, x, y)
# define isless(x, y)         __MATH_TG2(__isless, x, y)
# define islessequal(x, y)    __MATH_TG2(__islessequal, x, y)
# define islessgreater(x, y)  __MATH_TG2(__islessgreater, x, y)
# define isunordered(x, y)    __MATH_TG2(__isunordered, x, y)
#endif

__BEGIN_DECLS
double      acos(double);
float       acosf(float);
long double acosl(long double);
double      acosh(double);
float       acoshf(float);
long double acoshl(long double);
double      asin(double);
float       asinf(float);
long double asinl(long double);
double      asinh(double);
float       asinhf(float);
long double asinhl(long double);
double      atan(double);
float       atanf(float);
long double atanl(long double);
double      atan2(double, double);
float       atan2f(float, float);
long double atan2l(long double, long double);
double      atanh(double);
float       atanhf(float);
long double atanhl(long double);
double      cbrt(double);
float       cbrtf(float);
long double cbrtl(long double);
double      ceil(double);
float       ceilf(float);
long double ceill(long double);
double      copysign(double, double);
float       copysignf(float, float);
long double copysignl(long double, long double);
double      cos(double);
float       cosf(float);
long double cosl(long double);
double      cosh(double);
float       coshf(float);
long double coshl(long double);
double      erf(double);
float       erff(float);
long double erfl(long double);
double      erfc(double);
float       erfcf(float);
long double erfcl(long double);
double      exp(double);
float       expf(float);
long double expl(long double);
double      exp2(double);
float       exp2f(float);
long double exp2l(long double);
double      expm1(double);
float       expm1f(float);
long double expm1l(long double);
double      fabs(double);
float       fabsf(float);
long double fabsl(long double);
double      fdim(double, double);
float       fdimf(float, float);
long double fdiml(long double, long double);
double      floor(double);
float       floorf(float);
long double floorl(long double);
double      fma(double, double, double);
float       fmaf(float, float, float);
long double fmal(long double, long double, long double);
double      fmax(double, double);
float       fmaxf(float, float);
long double fmaxl(long double, long double);
double      fmin(double, double);
float       fminf(float, float);
long double fminl(long double, long double);
double      fmod(double, double);
float       fmodf(float, float);
long double fmodl(long double, long double);
double      frexp(double, int *);
float       frexpf(float, int *);
long double frexpl(long double, int *);
double      hypot(double, double);
float       hypotf(float, float);
long double hypotl(long double, long double);
int         ilogb(double);
int         ilogbf(float);
int         ilogbl(long double);
double      ldexp(double, int);
float       ldexpf(float, int);
long double ldexpl(long double, int);
double      lgamma(double);
float       lgammaf(float);
long double lgammal(long double);
long long   llrint(double);
long long   llrintf(float);
long long   llrintl(long double);
long long   llround(double);
long long   llroundf(float);
long long   llroundl(long double);
double      log(double);
float       logf(float);
long double logl(long double);
double      log10(double);
float       log10f(float);
long double log10l(long double);
double      log1p(double);
float       log1pf(float);
long double log1pl(long double);
double      log2(double);
float       log2f(float);
long double log2l(long double);
double      logb(double);
float       logbf(float);
long double logbl(long double);
long        lrint(double);
long        lrintf(float);
long        lrintl(long double);
long        lround(double);
long        lroundf(float);
long        lroundl(long double);
double      modf(double, double *);
float       modff(float, float *);
long double modfl(long double, long double *);
double      nan(const char *);
float       nanf(const char *);
long double nanl(const char *);
double      nearbyint(double);
float       nearbyintf(float);
long double nearbyintl(long double);
double      nextafter(double, double);
float       nextafterf(float, float);
long double nextafterl(long double, long double);
double      nexttoward(double, long double);
float       nexttowardf(float, long double);
long double nexttowardl(long double, long double);
double      pow(double, double);
float       powf(float, float);
long double powl(long double, long double);
double      remainder(double, double);
float       remainderf(float, float);
long double remainderl(long double, long double);
double      remquo(double, double, int *);
float       remquof(float, float, int *);
long double remquol(long double, long double, int *);
double      rint(double);
float       rintf(float);
long double rintl(long double);
double      round(double);
float       roundf(float);
long double roundl(long double);
double      scalbln(double, long);
float       scalblnf(float, long);
long double scalblnl(long double, long);
double      scalbn(double, int);
float       scalbnf(float, int);
long double scalbnl(long double, int);
double      sin(double);
float       sinf(float);
long double sinl(long double);
double      sinh(double);
float       sinhf(float);
long double sinhl(long double);
double      sqrt(double);
float       sqrtf(float);
long double sqrtl(long double);
double      tan(double);
float       tanf(float);
long double tanl(long double);
double      tanh(double);
float       tanhf(float);
long double tanhl(long double);
double      tgamma(double);
float       tgammaf(float);
long double tgammal(long double);
double      trunc(double);
float       truncf(float);
long double truncl(long double);

#if __XSI_VISIBLE
double j0(double);
double j1(double);
double jn(int, double);
double y0(double);
double y1(double);
double yn(int, double);
extern int signgam;
#endif
__END_DECLS

#endif
