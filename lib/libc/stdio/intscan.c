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

/* __intscan: read an integer (strtol syntax) for scanf.  Overflow
 * saturates at lim (or -lim-1 / -lim for negative signed values, which
 * the caller encodes in lim as for strtol). */
#include <errno.h>
#include <limits.h>
#include "stdio_impl.h"

static int digitval(int c)
{
	if ((unsigned)c - '0' < 10) return c - '0';
	if ((unsigned)(c | 32) - 'a' < 26) return (c | 32) - 'a' + 10;
	return 99;
}

unsigned long long __intscan(FILE *f, unsigned base, int pok, unsigned long long lim)
{
	int c, neg = 0, any = 0, over = 0;
	unsigned long long x = 0;
	(void)pok;
	if (base > 36 || base == 1) { errno = EINVAL; return 0; }
	while (((c = shgetc(f)) == ' ') || (unsigned)c - '\t' < 5);
	if (c == '+' || c == '-') { neg = -(c == '-'); c = shgetc(f); }
	if ((base == 0 || base == 16) && c == '0') {
		c = shgetc(f);
		if ((c | 32) == 'x') {
			c = shgetc(f);
			if (digitval(c) >= 16) {
				/* "0x" without hex digits: the value is the 0 */
				shunget(f);
				if (pok) shunget(f); else { shlim(f, 0); return 0; }
				return 0;
			}
			base = 16;
		} else {
			any = 1;
			if (base == 0) base = 8;
		}
	}
	if (!base) base = 10;
	for (; digitval(c) < (int)base; c = shgetc(f)) {
		unsigned d = (unsigned)digitval(c);
		any = 1;
		if (over) continue;
		if (x > (ULLONG_MAX - d) / base) over = 1;
		else x = x * base + d;
	}
	shunget(f);
	if (!any) { shlim(f, 0); return 0; }
	if (over || x > lim + (neg && lim != ULLONG_MAX)) {
		errno = ERANGE;
		if (lim == ULLONG_MAX || !neg) return lim;
		return -lim - 1;
	}
	return neg ? -x : x;
}
