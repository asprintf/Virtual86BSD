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

/* Rounding to the result type, and the error results, with errno. */
#include "libm.h"

double __math_d(long double y)
{
	double r = (double)y;
	if (__ld_nonfinite(r)) {                       /* infinite or NaN */
		if (!__ld_nonfinite(y))                 /* finite y: overflow */
			errno = ERANGE;
	} else if (r == 0 && y != 0)
		errno = ERANGE;                 /* underflow to zero */
	return r;
}

float __math_f(long double y)
{
	float r = (float)y;
	if (__ld_nonfinite(r)) {
		if (!__ld_nonfinite(y))
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
	if (__ld_nonfinite(y) && y == y)
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
