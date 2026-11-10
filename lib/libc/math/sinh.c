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

#define BIG 11356.5234062941439488L    /* ln(LDBL_MAX) */

/* e^|x| / 2 near overflow: (e^(|x|/2) / 2) e^(|x|/2) */
static long double halfexp(long double ax)
{
	long double w;
	if (ax > BIG + 0.7L)
		return __math_oflow(0);
	w = __expl(ax / 2);
	return __math_l(w / 2 * w);
}

long double sinhl(long double x)
{
	long double ax = __ld_abs(x), t, r;
	if (__ld_nonfinite(x))
		return x + x;
	if (ax < 0x1p-32L)
		return __math_l(x);
	if (ax < 1) {
		t = __expm1l(ax);
		r = (t + t / (t + 1)) / 2;
	} else if (ax < 23)
		r = (__expl(ax) - 1 / __expl(ax)) / 2;   /* e^-ax: not cancelling */
	else if (ax < BIG - 1)
		r = __expl(ax) / 2;
	else
		r = halfexp(ax);
	return x < 0 ? -r : r;
}

long double coshl(long double x)
{
	long double ax = __ld_abs(x), t;
	if (x != x)
		return x + x;
	if (__ld_nonfinite(ax))
		return ax;
	if (ax < 0x1p-33L)
		return 1;
	if (ax < 0.3466L) {
		t = __expm1l(ax);
		return 1 + t * t / (2 * (1 + t));
	}
	if (ax < 23) {
		t = __expl(ax);
		return (t + 1 / t) / 2;
	}
	if (ax < BIG - 1)
		return __expl(ax) / 2;
	return halfexp(ax);
}

long double tanhl(long double x)
{
	long double ax = __ld_abs(x), t, r;
	if (x != x)
		return x + x;
	if (ax < 0x1p-32L)
		return __math_l(x);
	if (ax > 23)
		r = 1;
	else if (ax >= 0.55L) {
		t = __expm1l(2 * ax);
		r = 1 - 2 / (t + 2);
	} else {
		t = __expm1l(-2 * ax);
		r = -t / (t + 2);
	}
	return x < 0 ? -r : r;
}

__MATH_WRAP1(sinh)
__MATH_WRAP1(cosh)
__MATH_WRAP1(tanh)
