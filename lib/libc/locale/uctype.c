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

/* The Unicode character data of the built-in C.UTF-8 locale. */
#include <stdint.h>
#include <stddef.h>
#include "locale_impl.h"
#include "uctype_class.h"

static int in(const uint32_t *r, size_t n, uint32_t c)
{
	size_t lo = 0, hi = n / 2;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if (c < r[2 * m]) hi = m;
		else if (c > r[2 * m + 1]) lo = m + 1;
		else return 1;
	}
	return 0;
}
#define IN(t, c) in(t, sizeof t / sizeof *t, c)

int __uctype_class(int cls, uint32_t c)
{
	switch (cls) {
	case W_ALNUM: return IN(u_alpha, c) || c - '0' < 10;
	case W_ALPHA: return IN(u_alpha, c);
	case W_BLANK: return IN(u_blank, c);
	case W_CNTRL: return IN(u_cntrl, c);
	case W_DIGIT: return c - '0' < 10;
	case W_GRAPH: return IN(u_print, c) && !IN(u_space, c);
	case W_LOWER: return IN(u_lower, c);
	case W_PRINT: return IN(u_print, c);
	case W_PUNCT: return IN(u_print, c) && !IN(u_space, c) && !IN(u_alpha, c) && c - '0' >= 10;
	case W_SPACE: return IN(u_space, c);
	case W_UPPER: return IN(u_upper, c);
	case W_XDIGIT: return c - '0' < 10 || (c | 32) - 'a' < 6;
	}
	return 0;
}

/* the last y >= c with IN(r, y) == IN(r, c) */
static uint32_t edge(const uint32_t *r, size_t n, uint32_t c)
{
	size_t lo = 0, hi = n / 2;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if (c < r[2 * m]) hi = m;
		else if (c > r[2 * m + 1]) lo = m + 1;
		else return r[2 * m + 1];
	}
	return lo < n / 2 ? r[2 * lo] - 1 : 0x10ffff;
}
#define E(t) do { uint32_t e_ = edge(t, sizeof t / sizeof *t, c); if (e_ < y) y = e_; } while (0)

/* the last y >= c such that __uctype_class(cls, ...) is constant on [c, y] */
uint32_t __uctype_run(int cls, uint32_t c)
{
	uint32_t y = 0x10ffff;
	if (c < 0x80) return c;
	switch (cls) {
	case W_ALNUM: case W_ALPHA: E(u_alpha); break;
	case W_BLANK: E(u_blank); break;
	case W_CNTRL: E(u_cntrl); break;
	case W_GRAPH: E(u_print); E(u_space); break;
	case W_LOWER: E(u_lower); break;
	case W_PRINT: E(u_print); break;
	case W_PUNCT: E(u_print); E(u_space); E(u_alpha); break;
	case W_SPACE: E(u_space); break;
	case W_UPPER: E(u_upper); break;
	}
	return y;
}
