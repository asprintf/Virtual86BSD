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
#include <limits.h>
#include <fenv.h>

/* the exponent of a finite nonzero x, subnormals included */
static int expo(long double x)
{
	int e = __ld_exp(x);
	if (e == 0)
		e = __ld_exp(x * 0x1p64L) - 64;
	return e - 16383;
}

int ilogbl(long double x)
{
	int r;
	if (x != x)
		r = FP_ILOGBNAN;
	else if (x == 0)
		r = FP_ILOGB0;
	else if (__ld_nonfinite(x))
		r = INT_MAX;
	else
		return expo(x);
	errno = EDOM;
	feraiseexcept(FE_INVALID);
	return r;
}

int ilogb(double x) { return ilogbl(x); }
int ilogbf(float x) { return ilogbl(x); }

long double logbl(long double x)
{
	if (x != x)
		return x + x;
	if (x == 0)
		return __math_divzero(1);     /* a pole error: -HUGE_VALL, ERANGE, FE_DIVBYZERO (POSIX) */
	if (__ld_nonfinite(x))
		return __ld_abs(x);
	return expo(x);
}

double logb(double x) { return logbl(x); }
float logbf(float x) { return logbl(x); }
