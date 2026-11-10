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

/* tgamma in long double:
 *   x >= 12:  sqrt(2 pi) x^(x - 1/2) e^-x e^(Stirling tail), the power
 *             as (x^((x - 1/2)/2) e^-x) x^((x - 1/2)/2) against overflow
 *   0 < x < 12: Gamma(x + n) / (x (x + 1) ... (x + n - 1))
 *   x < 0: pi / (sin(pi x) Gamma(1 - x))
 */
#include "libm.h"

#define SQRT_2PI 2.50662827463100050241576528481104525L

static long double gamma_pos(long double x)      /* x > 0, finite, < 1756 */
{
	long double p, q = 1;
	if (x < 12) {
		while (x < 12) {
			q *= x;
			x += 1;
		}
	}
	p = powl(x, (x - 0.5L) / 2);
	return p * __expl(-x) * p * SQRT_2PI * __expl(__stirling_tail(x)) / q;
}

long double tgammal(long double x)
{
	long double s, g;
	if (x != x)
		return x + x;
	if (__ld_nonfinite(x))
		return x > 0 ? x : __math_invalid(x);
	if (x == 0)
		return __math_divzero(__signbitl(x));
	if (x > 0) {
		if (x > 1755.5L)
			return __math_oflow(0);
		if (x < 0x1p-70L)
			return __math_l(1 / x);
		return __math_l(gamma_pos(x));
	}
	if (__x87_rint(x) == x)
		return __math_invalid(x);
	s = __sinpil(x);
	if (x < -1754)
		return __math_uflow(s < 0);
	if (x > -0x1p-70L)
		return __math_l(1 / x);
	g = gamma_pos(1 - x);
	return __math_l(__PI / (s * g));
}

__MATH_WRAP1(tgamma)
