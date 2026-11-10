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

#include <wchar.h>
#include <string.h>
#include <errno.h>
#include "locale_impl.h"
#include "mb_impl.h"
#include "../iconv/iconv_impl.h"

size_t mbrtowc(wchar_t *__RESTRICT pwc, const char *__RESTRICT s, size_t n, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	return __mbrtowc_l(pwc, s, n, st ? st : &internal, CURRENT_CTYPE);
}

size_t mbrlen(const char *__RESTRICT s, size_t n, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	return __mbrtowc_l(0, s, n, st ? st : &internal, CURRENT_CTYPE);
}

int mbsinit(const mbstate_t *st)
{
	return !st || !st->__st[0];
}

size_t wcrtomb(char *__RESTRICT s, wchar_t wc, mbstate_t *__RESTRICT st)
{
	(void)st;
	if (!s) return 1;   /* no shift states: the initial state takes one byte (NUL) */
	return __wcrtomb_l(s, wc, CURRENT_CTYPE);
}

wint_t btowc(int c)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	mbstate_t st = { { 0, 0 } };
	char b = (char)c;
	wchar_t w;
	if (c == -1 || (unsigned)c > 0xff) return WEOF;
	/* ASCII is itself (glibc: also in Shift_JIS, where mbrtowc gives the
	 * yen sign for 0x5c); GBK's 0x80 (the euro sign) is no single byte */
	if (c < 0x80) return (wint_t)c;
	if (ct->cs == CS_MB && ct->mbkind == GBK && c == 0x80) return WEOF;
	if (__mbrtowc_l(&w, &b, 1, &st, ct) > 1) return WEOF;
	return (wint_t)w;
}

int wctob(wint_t c)
{
	char b[8];
	if (c == WEOF) return -1;
	if (c < 0x80) return (int)c;
	if (CURRENT_CTYPE->cs == CS_UTF8) return -1;
	if (__wcrtomb_l(b, (wchar_t)c, CURRENT_CTYPE) != 1) return -1;
	return (unsigned char)b[0];
}

size_t mbsrtowcs(wchar_t *__RESTRICT ws, const char **__RESTRICT src, size_t n, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	const struct __lcat *ct = CURRENT_CTYPE;
	const char *s = *src;
	size_t k = 0, r;
	wchar_t w;
	if (!st) st = &internal;
	for (; !ws || k < n; k++) {
		r = __mbrtowc_l(&w, s, (size_t)-1 / 2, st, ct);
		if (r == (size_t)-1) { if (ws) *src = s; return r; }
		if (ws) ws[k] = w;
		if (!r) { if (ws) *src = 0; return k; }
		s += r;
	}
	*src = s;
	return k;
}

size_t wcsrtombs(char *__RESTRICT s, const wchar_t **__RESTRICT src, size_t n, mbstate_t *__RESTRICT st)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	const wchar_t *w = *src;
	char buf[8];
	size_t k = 0, r;
	(void)st;
	for (;; w++) {
		if (!*w) {
			if (s) { if (k < n) { s[k] = 0; *src = 0; } }
			return k;
		}
		r = __wcrtomb_l(buf, *w, ct);
		if (r == (size_t)-1) { if (s) *src = w; return r; }
		if (s) {
			if (k + r > n) { *src = w; return k; }
			memcpy(s + k, buf, r);
		}
		k += r;
	}
}
