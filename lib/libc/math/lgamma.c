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

/* lgamma: log |Gamma(x)| in long double; the sign of Gamma(x) goes to
 * signgam (through __signgam, see arch/x86_64/x87.s).
 *   |t| <= 1/2:  lgamma(2 + t) = (1 - gamma) t + sum_k>=2 (-1)^k (zeta(k) - 1) t^k / k
 *   lgamma(1 + t) = lgamma(2 + t) - log1p(t)
 *   2.5 <= x < 13: down to [1.5, 2.5) by Gamma(x + 1) = x Gamma(x)
 *   x >= 13: Stirling's series
 *   x < 0: Gamma(x) Gamma(1 - x) = pi / sin(pi x)
 */
#include "libm.h"
#include "zeta.h"

extern int __signgam;

/* lgamma(2 + t), |t| <= 1/2 */
static long double l2(long double t)
{
	long double s = 0;
	int k;
	for (k = 41; k >= 2; k--)
		s = s * t + (k & 1 ? -__zm1[k - 2] : __zm1[k - 2]) / k;
	return t * ((1 - __EGAMMA) + t * s);
}

long double __stirling_tail(long double x);

#define HALF_LOG_2PI 0.918938533204672741780329736405617640L

static long double lgamma_pos(long double x)     /* x > 0 finite */
{
	long double p, t;
	int n;
	if (x < 0.5L)
		return l2(x) - __log1pl(x) - __x87_yl2x(x, __LN2);
	if (x < 1.5L) {
		t = x - 1;
		return l2(t) - __log1pl(t);
	}
	if (x < 2.5L)
		return l2(x - 2);
	if (x < 13) {               /* x = y + n, y in [1.5, 2.5) */
		n = (int)(x - 1.5L);
		t = x - n;          /* exact */
		p = t;
		while (--n > 0)
			p *= t + n;
		return l2(t - 2) + __x87_yl2x(p, __LN2);
	}
	return (x - 0.5L) * (__x87_yl2x(x, __LN2) - 1) - 0.5L + HALF_LOG_2PI + __stirling_tail(x);
}

long double __lgammal_r(long double x, int *sign)
{
	long double s;
	*sign = 1;
	if (x != x)
		return x + x;
	if (__ld_nonfinite(x))
		return __ld_abs(x);
	if (x == 0) {
		if (__signbitl(x))
			*sign = -1;
		return __math_divzero(0);
	}
	if (x > 0)
		return lgamma_pos(x);
	if (x > -1) {               /* Gamma(x) = Gamma(1 + x) / x */
		*sign = -1;
		return lgamma_pos(1 + x) - __x87_yl2x(-x, __LN2);
	}
	s = __sinpil(x);
	if (s == 0)
		return __math_divzero(0);
	if (s < 0)
		*sign = -1;
	/* log(pi / |sin(pi x)|) - lgamma(1 - x) */
	return __x87_yl2x(__PI / __ld_abs(s), __LN2) - lgamma_pos(1 - x);
}

long double lgammal(long double x)
{
	long double r = __lgammal_r(x, &__signgam);
	return __ld_nonfinite(x) ? r : __math_l(r);
}

double lgamma(double x)
{
	return __math_d(__lgammal_r(x, &__signgam));
}

float lgammaf(float x)
{
	return __math_f(__lgammal_r(x, &__signgam));
}
