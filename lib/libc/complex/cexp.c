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

long double _Complex cexpl(long double _Complex z)
{
	long double x = __cre(z), y = __cim(z), e, c, s;
	if (y == 0)
		return __cpack(expl(x), y);
	if (__ld_nonfinite(y)) {                       /* y infinite or NaN */
		if (__ld_nonfinite(x) && x == x)        /* x infinite */
			return x > 0 ? __cpack(x, y - y) : __cpack(0, 0);
		return __cpack(y - y, y - y);
	}
	if (x != x)
		return __cpack(x, x);
	c = cosl(y);
	s = sinl(y);
	if (__ld_nonfinite(x))                          /* +-inf: exp(x) cis y */
		return x > 0 ? __cpack(x * c, x * s) : __cpack(0 * c, 0 * s);
	if (x > 11356) {                         /* e^x overflows, maybe not the parts */
		e = expl(x / 2);
		return __cpack(e * c * e, e * s * e);
	}
	e = expl(x);
	return __cpack(e * c, e * s);
}
__CWRAP(cexp)
