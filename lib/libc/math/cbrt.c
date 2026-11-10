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

/* an approximation 2^(log2|x| / 3) by the x87, then one Halley step
 * y (y^3 + 2x) / (2y^3 + x) on a scaled x, which triples the accuracy */
long double cbrtl(long double x)
{
	long double ax = __ld_abs(x), y, y3, n, m;
	int e;
	if (x == 0 || __ld_nonfinite(x))
		return x + x;
	/* ax = m 2^(3n), m in [1, 8) */
	e = __ld_exp(ax) - 16383;
	if (e < -16382) {               /* subnormal */
		ax *= 0x1p66L;
		e = __ld_exp(ax) - 16383 - 66;
	}
	n = (long double)((e + 3 * 6000) / 3 - 6000);
	m = __x87_scale(ax, -3 * n);
	y = __x87_scale(1 + __x87_2xm1(__x87_yl2x(m, 1.0L / 3) - 1), 1);
	y3 = y * y * y;
	y = y * (y3 + 2 * m) / (2 * y3 + m);
	y = __x87_scale(y, n);
	return x < 0 ? -y : y;
}

__MATH_WRAP1(cbrt)
