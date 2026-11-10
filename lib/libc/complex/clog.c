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

/* log |x + iy|, accurately also near |z| = 1 */
static long double loghypot(long double x, long double y)
{
	long double ax = fabsl(x), ay = fabsl(y), t, h1, l1, h2, l2;
	if ((__ld_nonfinite(ax) && ax == ax) || (__ld_nonfinite(ay) && ay == ay))
		return HUGE_VALL;
	if (ax != ax || ay != ay)
		return ax + ay;
	if (ax < ay) {
		t = ax; ax = ay; ay = t;
	}
	if (ax == 0)
		return -1.0L / ax;              /* pole: FE_DIVBYZERO */
	if (ax > 0.5L && ax < 1.5L && ay < 0x1p-1L) {
		/* (ax^2 - 1) + ay^2 exactly enough, then log1p */
		__two_prod(ax, ax, &h1, &l1);
		__two_prod(ay, ay, &h2, &l2);
		t = h1 - 1;                     /* exact */
		t = ((t + h2) + l1) + l2;
		if (fabsl(t) < 0.5L)
			return log1pl(t) / 2;
	}
	return logl(hypotl(ax, ay));
}

long double _Complex clogl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z);
	return __cpack(loghypot(x, y), atan2l(y, x));
}
__CWRAP(clog)

/* z^w = e^(w log z) */
long double _Complex cpowl(long double _Complex z, long double _Complex w)
{
	long double _Complex l = clogl(z);
	long double a = __cre(l), b = __cim(l), c = __cre(w), d = __cim(w);
	return cexpl(__cpack(a * c - b * d, a * d + b * c));
}

double _Complex cpow(double _Complex z, double _Complex w) { return cpowl(z, w); }
float _Complex cpowf(float _Complex z, float _Complex w) { return cpowl(z, w); }
