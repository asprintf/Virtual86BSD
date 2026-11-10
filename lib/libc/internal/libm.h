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

/* Internal: the mathematics library.
 *
 * The functions compute in the x87 80-bit format (64-bit significand)
 * and round once to the result's type, so float and double results are
 * almost always correctly rounded and long double results are within a
 * few units in the last place.  The x87 instructions (fsin, fyl2x,
 * f2xm1, ...) are reached through the small functions of
 * arch/x86_64/x87.s, since c17 has no inline assembly.
 *
 * Errors: math_errhandling is MATH_ERRNO | MATH_ERREXCEPT, so the
 * functions set errno (EDOM for domain errors, ERANGE for pole errors,
 * overflow and underflow) besides raising the exceptions.
 */
#ifndef _LIBM_H
#define _LIBM_H
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <errno.h>

union __ldbits { long double f; struct { uint64_t m; uint16_t se; } i; };
union __dbits { double f; uint64_t i; };
union __fbits { float f; uint32_t i; };

/* x87 primitives (arch/x86_64/x87.s); the arguments must be in the
 * instruction's domain. */
long double __x87_sin(long double);           /* |x| < 2^63 */
long double __x87_cos(long double);
long double __x87_tan(long double);           /* fptan */
long double __x87_atan2(long double, long double);  /* fpatan: atan(y/x), quadrant of (x, y) */
long double __x87_yl2x(long double, long double);   /* y * log2(x), x > 0 */
long double __x87_yl2xp1(long double, long double); /* y * log2(1 + x), |x| < 1 - sqrt(2)/2 */
long double __x87_2xm1(long double);          /* 2^x - 1, |x| <= 1 */
long double __x87_scale(long double, long double);  /* x * 2^trunc(n), exact */
long double __x87_sqrt(long double);
long double __x87_rint(long double);          /* in the current rounding mode */
long double __x87_fmod(long double, long double, int *);       /* fprem; *q = low 3 quotient bits */
long double __x87_remainder(long double, long double, int *);  /* fprem1 */
double __sse_sqrt(double);
float __sse_sqrtf(float);

/* rounding to the result type, with errno for overflow and underflow:
 * y is the exact result rounded to long double */
double __math_d(long double y);
float __math_f(long double y);
long double __math_l(long double y);   /* y may be infinite or zero after overflow or underflow */
/* error results: a domain error (NaN, EDOM), a pole error (+-inf, ERANGE) */
long double __math_invalid(long double x);
long double __math_divzero(int neg);
long double __math_oflow(int neg);
long double __math_uflow(int neg);

/* argument reduction: x = n pi/2 + r, |r| <= pi/4 (about); returns n */
int __rem_pio2l(long double x, long double *r);

/* cores in long double */
long double __expl(long double x);           /* no error handling; |x| moderate */
long double __exp2l_hl(long double hi, long double lo, long double s);  /* s * 2^(hi + lo) */
void __log2l_hl(long double x, long double *hi, long double *lo);  /* log2(x), x > 0 finite */
long double __expm1l(long double x);
long double __log1pl(long double x);
long double __lgammal_r(long double x, int *sign);
long double __sinpil(long double x);         /* sin(pi x), exact reduction */
long double __cospil(long double x);
long double __stirling_tail(long double x);  /* lgamma(x) - Stirling's main terms, x >= 12 */

/* double-long-double helpers (Dekker): exact products and sums */
static inline void __two_prod(long double a, long double b, long double *hi, long double *lo)
{
	const long double c = 4294967297.0L;   /* 2^32 + 1 */
	long double p = a * b, t, ah, al, bh, bl;
	t = c * a; ah = t - (t - a); al = a - ah;
	t = c * b; bh = t - (t - b); bl = b - bh;
	*hi = p;
	*lo = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
}

static inline void __two_sum(long double a, long double b, long double *hi, long double *lo)
{
	long double s = a + b, bb = s - a;
	*hi = s;
	*lo = (a - (s - bb)) + (b - bb);
}

/* bits of long double */
static inline int __ld_exp(long double x)       /* biased exponent, 0..0x7fff */
{
	union __ldbits u = {x};
	return u.i.se & 0x7fff;
}

/* infinite or NaN, without the FE_INVALID that x - x != 0 raises for an
   infinity */
static inline int __ld_nonfinite(long double x)
{
	return __ld_exp(x) == 0x7fff;
}

static inline long double __ld_abs(long double x)
{
	union __ldbits u = {x};
	u.i.se &= 0x7fff;
	return u.f;
}

#define __LN2HI  0x0.B17217F7D1CFp0L                 /* 48 bits */
#define __LN2LO  0x7.9ABC9E3B39803F2F6AF40F3432p-52L
#define __LOG2E  0x1.71547652B82FE1777D0FFDA0D23Ap0L
#define __LN2    0x0.B17217F7D1CF79ABC9E3B39803F2F6AFp0L
#define __LOG10_2 0x0.4D104D427DE7FBCC47C4ACD605BE48BCp0L
#define __PI     0x3.243F6A8885A308D313198A2E037073p0L
#define __PIO2   0x1.921FB54442D18469898CC51701B839A2p0L
#define __EGAMMA 0.57721566490153286060651209008240243L

/* the float and double functions of a long double one: the result
 * rounded once, with errno for overflow and underflow */
#define __MATH_WRAP1(N) \
	double N(double x) { return __math_d(N##l(x)); } \
	float N##f(float x) { return __math_f(N##l(x)); }
#define __MATH_WRAP2(N) \
	double N(double x, double y) { return __math_d(N##l(x, y)); } \
	float N##f(float x, float y) { return __math_f(N##l(x, y)); }

#endif
