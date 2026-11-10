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

/* Case mappings of the built-in C.UTF-8 locale. */
#include <stdint.h>
#include <stddef.h>
#include "locale_impl.h"
#include "uctype_map.h"

static int32_t lookup(const int32_t *runs, size_t nruns, uint32_t c)
{
	size_t lo = 0, hi = nruns, i, k;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if ((uint32_t)runs[4 * m] <= c) lo = m + 1; else hi = m;
	}
	/* runs of every other character can enclose a few others */
	for (i = lo, k = 0; i > 0 && k < 4; i--, k++) {
		const int32_t *r = runs + 4 * (i - 1);
		uint32_t d = c - (uint32_t)r[0];
		if (d < (uint32_t)(r[1] * r[3]) && d % (uint32_t)r[3] == 0) return r[2];
	}
	return 0;
}

uint32_t __uctype_toupper(uint32_t c) { return c + (uint32_t)lookup(u_toupper, sizeof u_toupper / 16, c); }
uint32_t __uctype_tolower(uint32_t c) { return c + (uint32_t)lookup(u_tolower, sizeof u_tolower / 16, c); }
