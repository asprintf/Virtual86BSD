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

#include "complex_impl.h"

/* Kahan's algorithm: t = sqrt((|x| + |z|)/2), then the other part y/(2t) */
void __csqrt_parts(long double x, long double y, long double *re, long double *im)
{
	long double t, ax = fabsl(x);
	int scale = 0;
	if (__ld_nonfinite(y) && y == y) {             /* y infinite */
		*re = HUGE_VALL;
		*im = y;
		return;
	}
	if (x != x) {
		*re = x;
		*im = y - y + x;
		return;
	}
	if (__ld_nonfinite(ax)) {                     /* x infinite */
		if (x > 0) {
			*re = x;
			*im = y == y ? copysignl(0, y) : y;
		} else {
			*re = y == y ? 0 : y;
			*im = copysignl(ax, y);
		}
		return;
	}
	if (y != y) {
		*re = *im = y;
		return;
	}
	if (x == 0 && y == 0) {
		*re = 0;
		*im = y;
		return;
	}
	if (ax > 0x1p16380L || fabsl(y) > 0x1p16380L) {
		ax *= 0x1p-4L; y *= 0x1p-4L;
		scale = 2;
	} else if (ax < 0x1p-16300L && fabsl(y) < 0x1p-16300L) {
		ax *= 0x1p64L; y *= 0x1p64L;
		scale = -32;
	}
	t = sqrtl((ax + hypotl(ax, y)) / 2);
	if (x >= 0) {
		*re = t;
		*im = y / (2 * t);
	} else {
		*re = fabsl(y) / (2 * t);
		*im = copysignl(t, y);
	}
	if (scale) {
		*re = ldexpl(*re, scale);
		*im = ldexpl(*im, scale);
	}
}

long double _Complex csqrtl(long double _Complex z)
{
	long double re, im;
	__csqrt_parts(__cre(z), __cim(z), &re, &im);
	return __cpack(re, im);
}
__CWRAP(csqrt)
