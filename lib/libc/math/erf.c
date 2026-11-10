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

/* erf and erfc in long double.
 *  |x| < 2: erf x = 2/sqrt(pi) e^(-x^2) sum 2^n x^(2n+1) / (1 3 5 ... (2n+1))
 *           (positive terms, no cancellation)
 *  x >= 0.5: erfc x = e^(-x^2)/sqrt(pi) / (x + (1/2)/(x + 1/(x + (3/2)/(x + ...))))
 *           (Laplace's continued fraction, evaluated backwards), with
 *           e^(-x^2) from x^2 = h + l exactly
 */
#include "libm.h"

#define SQRTPI_INV 0.564189583547756286948079451560772586L

static long double erf_series(long double x)     /* x >= 0 */
{
	long double x2 = x * x, t = x, s = x;
	int n;
	for (n = 1; n < 200; n++) {
		t *= 2 * x2 / (2 * n + 1);
		s += t;
		if (t < s * 0x1p-66L)
			break;
	}
	return 2 * SQRTPI_INV * __expl(-x2) * s;
}

static long double erfc_cf(long double x)
{
	long double f = x, h, l, e;
	int n = 20 + (int)(350 / (x * x));
	for (; n >= 1; n--)
		f = x + (n * 0.5L) / f;
	__two_prod(x, x, &h, &l);
	if (h > 11400)
		return 0;
	e = __expl(-h);
	return e * (1 - l) * SQRTPI_INV / f;
}

long double erfl(long double x)
{
	long double ax = __ld_abs(x), r;
	if (x != x)
		return x + x;
	if (ax < 0x1p-32L)
		return __math_l(2 * SQRTPI_INV * x);
	if (ax < 2)
		r = erf_series(ax);
	else
		r = ax > 7 ? 1 : 1 - erfc_cf(ax);
	return x < 0 ? -r : r;
}

long double erfcl(long double x)
{
	long double r;
	if (x != x)
		return x + x;
	if (x < 0.5L)
		return x < -7 ? 2 : 1 - erfl(x);
	if (__ld_nonfinite(x))
		return 0;
	r = erfc_cf(x);
	if (r == 0)
		return __math_uflow(0);
	return r;
}

__MATH_WRAP1(erf)
__MATH_WRAP1(erfc)
