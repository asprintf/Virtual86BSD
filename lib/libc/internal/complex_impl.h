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

/* Internal: the complex functions.  They compute in long double on the
 * real and imaginary parts (no complex arithmetic, which would need the
 * compiler's run-time support); the float and double functions are the
 * long double ones, each part rounded once. */
#ifndef _COMPLEX_IMPL_H
#define _COMPLEX_IMPL_H
#include <complex.h>
#include "libm.h"

typedef long double _Complex __ldc;
union __ldcu { __ldc z; long double a[2]; };

static inline __ldc __cpack(long double re, long double im)
{
	union __ldcu u;
	u.a[0] = re;
	u.a[1] = im;
	return u.z;
}

static inline long double __cre(__ldc z) { union __ldcu u; u.z = z; return u.a[0]; }
static inline long double __cim(__ldc z) { union __ldcu u; u.z = z; return u.a[1]; }

/* csqrt of x + iy, as parts */
void __csqrt_parts(long double x, long double y, long double *re, long double *im);

#define __CWRAP(N) \
	double _Complex N(double _Complex z) { return N##l(z); } \
	float _Complex N##f(float _Complex z) { return N##l(z); }

#endif
