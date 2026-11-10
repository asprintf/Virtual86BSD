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

/* the payload of "NAN(n-char-sequence)": the digits as strtoull reads
 * them (decimal, octal or hexadecimal), or 0 */
static uint64_t payload(const char *s)
{
	uint64_t v = 0;
	unsigned base = 10, d;
	if (*s == '0') {
		base = 8;
		s++;
		if (*s == 'x' || *s == 'X') {
			base = 16;
			s++;
		}
	}
	for (; *s; s++) {
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (*s >= 'a' && *s <= 'f')
			d = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F')
			d = *s - 'A' + 10;
		else
			return 0;
		if (d >= base)
			return 0;
		v = v * base + d;
	}
	return v;
}

double nan(const char *s)
{
	union __dbits u;
	u.i = 0x7ff8000000000000ULL | (payload(s) & ((1ULL << 51) - 1));
	return u.f;
}

float nanf(const char *s)
{
	union __fbits u;
	u.i = 0x7fc00000 | (uint32_t)(payload(s) & ((1u << 22) - 1));
	return u.f;
}

long double nanl(const char *s)
{
	union __ldbits u;
	u.i.m = 0xc000000000000000ULL | (payload(s) & ((1ULL << 62) - 1));
	u.i.se = 0x7fff;
	return u.f;
}
