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

/* e^x = 2^n e^r, n = rint(x log2 e), r = x - n ln 2 (ln 2 in two parts,
 * n * __LN2HI exact), e^r = 2^(r log2 e) by f2xm1 */
long double __expl(long double x)
{
	long double n = __x87_rint(x * __LOG2E);
	long double r = (x - n * __LN2HI) - n * __LN2LO;
	return __x87_scale(1 + __x87_2xm1(r * __LOG2E), n);
}

long double expl(long double x)
{
	if (x != x)
		return x + x;
	if (x > 11356.6L) {
		if (__ld_nonfinite(x))
			return x;
		return __math_oflow(0);
	}
	if (x < -11400) {
		if (__ld_nonfinite(x))
			return 0;
		return __math_uflow(0);
	}
	if (__ld_abs(x) < 0x1p-66L)
		return 1 + x;
	return __math_l(__expl(x));
}

__MATH_WRAP1(exp)
