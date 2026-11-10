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

/* x * 2^n: fscale is exact in long double, so double and float results
 * are rounded once */
#include "libm.h"

long double scalblnl(long double x, long n)
{
	long double r;
	if (x == 0 || __ld_nonfinite(x))
		return x + x;
	r = __x87_scale(x, (long double)n);
	if (r == 0)
		errno = ERANGE;
	return __math_l(r);
}

long double scalbnl(long double x, int n) { return scalblnl(x, n); }
long double ldexpl(long double x, int n) { return scalblnl(x, n); }

static long double scale(long double x, long n)
{
	if (x == 0 || __ld_nonfinite(x))
		return x + x;
	return __x87_scale(x, (long double)n);
}

double scalbln(double x, long n) { return __math_d(scale(x, n)); }
double scalbn(double x, int n) { return __math_d(scale(x, n)); }
double ldexp(double x, int n) { return __math_d(scale(x, n)); }
float scalblnf(float x, long n) { return __math_f(scale(x, n)); }
float scalbnf(float x, int n) { return __math_f(scale(x, n)); }
float ldexpf(float x, int n) { return __math_f(scale(x, n)); }
