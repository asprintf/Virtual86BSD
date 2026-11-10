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

#include "libm.h"

long double hypotl(long double x, long double y)
{
	long double ax = __ld_abs(x), ay = __ld_abs(y), t;
	int e;
	if (__ld_nonfinite(ax) && ax == ax)
		return ax;              /* inf, even if y is NaN */
	if (__ld_nonfinite(ay) && ay == ay)
		return ay;
	if (x != x || y != y)
		return x + y;
	if (ax < ay) {
		t = ax; ax = ay; ay = t;
	}
	if (ay == 0)
		return ax;
	/* scale ax to [1, 2) */
	e = __ld_exp(ax) - 16383;
	if (e < -16382)
		e = -16382;
	ax = __x87_scale(ax, -e);
	ay = __x87_scale(ay, -e);
	return __math_l(__x87_scale(__x87_sqrt(ax * ax + ay * ay), e));
}

/* double: no scaling needed in long double */
double hypot(double x, double y)
{
	long double ax = x, ay = y;
	if (__ld_nonfinite(ax) || __ld_nonfinite(ay))
		return hypotl(x, y);
	return __math_d(__x87_sqrt(ax * ax + ay * ay));
}

float hypotf(float x, float y)
{
	return __math_f(hypotl(x, y));
}
