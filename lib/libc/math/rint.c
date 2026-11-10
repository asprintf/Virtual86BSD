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

/* rint, nearbyint, lrint, llrint: in the current rounding mode, which
 * fesetround() keeps the same for the x87 unit and SSE */
#include "libm.h"
#include <fenv.h>

long double rintl(long double x) { return __x87_rint(x); }
double rint(double x) { return __x87_rint(x); }
float rintf(float x) { return __x87_rint(x); }

long double nearbyintl(long double x)
{
	int e = fetestexcept(FE_INEXACT);
	long double r = __x87_rint(x);
	if (!e)
		feclearexcept(FE_INEXACT);
	return r;
}

double nearbyint(double x) { return nearbyintl(x); }
float nearbyintf(float x) { return nearbyintl(x); }

long lrintl(long double x)
{
	long double t = __x87_rint(x);
	if (!(t >= -0x1p63L && t < 0x1p63L)) {
		errno = EDOM;             /* a domain error (POSIX): NaN, infinite, too large */
		feraiseexcept(FE_INVALID);
		return (long)(-0x7fffffffffffffffLL - 1);
	}
	return (long)t;
}

long lrint(double x) { return lrintl(x); }
long lrintf(float x) { return lrintl(x); }
long long llrintl(long double x) { return lrintl(x); }
long long llrint(double x) { return lrintl(x); }
long long llrintf(float x) { return lrintl(x); }
