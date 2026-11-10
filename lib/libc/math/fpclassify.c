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

/* The classification and comparison functions behind the macros of
 * <math.h> (used by compilers without the GCC built-ins).  Comparisons
 * of quiet NaNs raise no exception. */
#include "libm.h"

int __fpclassify(double x)
{
	union __dbits u = {x};
	int e = u.i >> 52 & 0x7ff;
	if (e == 0)
		return u.i << 1 ? FP_SUBNORMAL : FP_ZERO;
	if (e == 0x7ff)
		return u.i << 12 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

int __fpclassifyf(float x)
{
	union __fbits u = {x};
	int e = u.i >> 23 & 0xff;
	if (e == 0)
		return u.i << 1 ? FP_SUBNORMAL : FP_ZERO;
	if (e == 0xff)
		return u.i << 9 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

/* the x87 format has an explicit integer bit: encodings with the wrong
 * one (unnormals, pseudo-NaNs, pseudo-infinities) count as NaN */
int __fpclassifyl(long double x)
{
	union __ldbits u = {x};
	int e = u.i.se & 0x7fff;
	if (e == 0)
		return u.i.m ? FP_SUBNORMAL : FP_ZERO;
	if (!(u.i.m >> 63))
		return FP_NAN;
	if (e == 0x7fff)
		return u.i.m << 1 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

int __signbit(double x) { union __dbits u = {x}; return (int)(u.i >> 63); }
int __signbitf(float x) { union __fbits u = {x}; return (int)(u.i >> 31); }
int __signbitl(long double x) { union __ldbits u = {x}; return u.i.se >> 15; }

#define FUNCS(S, T) \
int __isfinite##S(T x) { return __fpclassify##S(x) >= FP_ZERO; } \
int __isnan##S(T x) { return __fpclassify##S(x) == FP_NAN; } \
int __isnormal##S(T x) { return __fpclassify##S(x) == FP_NORMAL; } \
int __isinf##S(T x) { return __fpclassify##S(x) != FP_INFINITE ? 0 : __signbit##S(x) ? -1 : 1; } \
int __isunordered##S(T x, T y) { return __isnan##S(x) || __isnan##S(y); } \
int __isgreater##S(T x, T y) { return !__isunordered##S(x, y) && x > y; } \
int __isgreaterequal##S(T x, T y) { return !__isunordered##S(x, y) && x >= y; } \
int __isless##S(T x, T y) { return !__isunordered##S(x, y) && x < y; } \
int __islessequal##S(T x, T y) { return !__isunordered##S(x, y) && x <= y; } \
int __islessgreater##S(T x, T y) { return !__isunordered##S(x, y) && x != y; }

FUNCS(, double)
FUNCS(f, float)
FUNCS(l, long double)
