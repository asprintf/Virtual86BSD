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

/* Single-byte codesets: their bytes 0x80-0xff (the lower half is ASCII). */
#include <string.h>
#include "locale_impl.h"
#include "charsets_data.h"

/* compare without case and punctuation */
static int same(const char *a, const char *b)
{
	for (;;) {
		int x, y;
		while (*a && !((*a | 32) >= 'a' && (*a | 32) <= 'z') && !(*a >= '0' && *a <= '9')) a++;
		while (*b && !((*b | 32) >= 'a' && (*b | 32) <= 'z') && !(*b >= '0' && *b <= '9')) b++;
		x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
		y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
		if (x != y) return 0;
		if (!x) return 1;
		a++; b++;
	}
}

const struct __charset *__charset_find(const char *name)
{
	unsigned i;
	for (i = 0; i < sizeof charsets / sizeof *charsets; i++)
		if (same(name, charsets[i].name)) return &charsets[i];
	for (i = 0; i < sizeof aliases / sizeof *aliases; i++)
		if (same(name, aliases[i].alias)) return &charsets[aliases[i].set];
	return 0;
}

int __charset_from_wc(const struct __charset *cs, uint32_t wc)
{
	unsigned i;
	if (wc < 0x80) return (int)wc;
	if (wc > 0xffff) return -1;
	for (i = 0; i < 128; i++) if (cs->hi[i] == wc) return (int)(0x80 + i);
	return -1;
}
