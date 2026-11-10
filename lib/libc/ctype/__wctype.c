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

/* Wide character classes and case mappings in a locale's LC_CTYPE.
 * wctype_t: 1-12 the standard classes, 100 + i class i of a locale file.
 * wctrans_t: 1 toupper, 2 tolower, 100 + i map i of a locale file. */
#include <string.h>
#include "locale_impl.h"

static const char *const std[W_NSTD] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph",
	"lower", "print", "punct", "space", "upper", "xdigit" };

static int ascii(int cls, unsigned c)
{
	switch (cls) {
	case W_ALNUM: return (c | 32) - 'a' < 26 || c - '0' < 10;
	case W_ALPHA: return (c | 32) - 'a' < 26;
	case W_BLANK: return c == ' ' || c == '\t';
	case W_CNTRL: return c < 0x20 || c == 0x7f;
	case W_DIGIT: return c - '0' < 10;
	case W_GRAPH: return c - 0x21 < 0x5e;
	case W_LOWER: return c - 'a' < 26;
	case W_PRINT: return c - 0x20 < 0x5f;
	case W_PUNCT: return c - 0x21 < 0x5e && !((c | 32) - 'a' < 26) && !(c - '0' < 10);
	case W_SPACE: return c == ' ' || c - '\t' < 5;
	case W_UPPER: return c - 'A' < 26;
	case W_XDIGIT: return c - '0' < 10 || (c | 32) - 'a' < 6;
	}
	return 0;
}

int __iswctype_l(unsigned wc, unsigned long t, const struct __lcat *ct)
{
	if (wc == 0xffffffffu || !t) return 0;
	if (t >= 100) {
		if (t - 100 >= ct->nclass) return 0;
		return __tab_bit((const uint32_t *)(const void *)ct->elem[72 + (t - 100)], wc);
	}
	if (t > W_NSTD) return 0;
	if (ct->wclass[t - 1]) return __tab_bit(ct->wclass[t - 1], wc);
	if (ct->cs == CS_UTF8) return wc < 0x80 ? ascii((int)t - 1, wc) : __uctype_class((int)t - 1, wc);
	return wc < 0x80 && ascii((int)t - 1, wc);
}

unsigned __towctrans_l(unsigned wc, unsigned long t, const struct __lcat *ct)
{
	const uint32_t *tab = 0;
	if (wc == 0xffffffffu) return wc;
	if (t >= 100) {
		if (t - 100 >= ct->nmap) return wc;
		tab = (const uint32_t *)(const void *)ct->elem[72 + ct->nclass + (t - 100)];
	} else if (t == 1 || t == 2) {
		tab = t == 1 ? ct->wtoupper : ct->wtolower;
		if (!tab) {
			if (wc < 0x80) {
				if (t == 1) return wc - 'a' < 26 ? wc & 0x5f : wc;
				return wc - 'A' < 26 ? wc | 32 : wc;
			}
			if (ct->cs != CS_UTF8) return wc;
			return t == 1 ? __uctype_toupper(wc) : __uctype_tolower(wc);
		}
	} else
		return wc;
	return wc + (unsigned)__tab_map(tab, wc);
}

unsigned long __wctype_l(const char *name, const struct __lcat *ct)
{
	const char *p;
	unsigned i;
	for (i = 0; i < W_NSTD; i++) if (!strcmp(name, std[i])) return i + 1;
	for (i = 0, p = ct->classnames; i < ct->nclass; i++, p += strlen(p) + 1)
		if (!strcmp(name, p)) return 100 + i;
	return 0;
}

unsigned long __wctrans_l(const char *name, const struct __lcat *ct)
{
	const char *p;
	unsigned i;
	if (!strcmp(name, "toupper")) return 1;
	if (!strcmp(name, "tolower")) return 2;
	for (i = 0, p = ct->mapnames; i < ct->nmap; i++, p += strlen(p) + 1)
		if (!strcmp(name, p)) return 100 + i;
	return 0;
}

/* a y >= wc such that __iswctype_l(x, t, ct) is the same for x in [wc, y] */
unsigned __iswctype_run(unsigned wc, unsigned long t, const struct __lcat *ct)
{
	if (!t || (t > W_NSTD && (t < 100 || t - 100 >= ct->nclass))) return 0x10ffff;
	if (t >= 100) return __tab_bit_run((const uint32_t *)(const void *)ct->elem[72 + (t - 100)], wc);
	if (ct->wclass[t - 1]) return __tab_bit_run(ct->wclass[t - 1], wc);
	if (wc < 0x80) return wc;
	if (ct->cs == CS_UTF8) return __uctype_run((int)t - 1, wc);
	return 0x10ffff;
}
