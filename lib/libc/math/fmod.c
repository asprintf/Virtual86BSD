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

/* fmod, remainder and remquo are exact: fprem and fprem1 compute them
 * in long double, also for the double and float arguments */
#include "libm.h"

static long double rem(long double x, long double y, int *q, int ieee)
{
	int b, neg = (x < 0) != (y < 0);
	*q = 0;
	if (x != x || y != y)
		return x + y;
	if (__ld_nonfinite(x) || y == 0)
		return __math_invalid(__ld_nonfinite(x) ? x : y);
	if (__ld_nonfinite(y) || x == 0)
		return x;
	x = ieee ? __x87_remainder(x, y, &b) : __x87_fmod(x, y, &b);
	*q = neg ? -b : b;
	return x;
}

long double fmodl(long double x, long double y) { int q; return rem(x, y, &q, 0); }
double fmod(double x, double y) { int q; return rem(x, y, &q, 0); }
float fmodf(float x, float y) { int q; return rem(x, y, &q, 0); }

long double remainderl(long double x, long double y) { int q; return rem(x, y, &q, 1); }
double remainder(double x, double y) { int q; return rem(x, y, &q, 1); }
float remainderf(float x, float y) { int q; return rem(x, y, &q, 1); }

long double remquol(long double x, long double y, int *q) { return rem(x, y, q, 1); }
double remquo(double x, double y, int *q) { return rem(x, y, q, 1); }
float remquof(float x, float y, int *q) { return rem(x, y, q, 1); }
