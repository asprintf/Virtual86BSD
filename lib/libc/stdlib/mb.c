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

/* The multibyte functions of <stdlib.h>, in the current locale. */
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include "locale_impl.h"
#include "mb_impl.h"

size_t __mb_cur_max(void)
{
	return (size_t)CURRENT_CTYPE->mbmax;
}

int mbtowc(wchar_t *__RESTRICT wc, const char *__RESTRICT s, size_t n)
{
	mbstate_t st = { { 0, 0 } };
	size_t r;
	if (!s) return 0;   /* no shift states */
	r = __mbrtowc_l(wc, s, n, &st, CURRENT_CTYPE);
	if (r == (size_t)-2) { errno = EILSEQ; return -1; }
	return (int)r;
}

int mblen(const char *s, size_t n) { return mbtowc(0, s, n); }

int wctomb(char *s, wchar_t wc)
{
	if (!s) return 0;
	return (int)__wcrtomb_l(s, wc, CURRENT_CTYPE);
}

size_t mbstowcs(wchar_t *__RESTRICT ws, const char *__RESTRICT s, size_t n)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	mbstate_t st = { { 0, 0 } };
	size_t k, r;
	wchar_t w;
	for (k = 0; !ws || k < n; k++) {
		r = __mbrtowc_l(&w, s, (size_t)-1 / 2, &st, ct);
		if (r == (size_t)-1) return r;
		if (ws) ws[k] = w;
		if (!r) return k;
		s += r;
	}
	return k;
}

size_t wcstombs(char *__RESTRICT s, const wchar_t *__RESTRICT ws, size_t n)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	char buf[8];
	size_t k = 0, r, i;
	for (;; ws++) {
		if (!*ws) { if (s && k < n) s[k] = 0; return k; }
		if ((r = __wcrtomb_l(buf, *ws, ct)) == (size_t)-1) return r;
		if (s) {
			if (k + r > n) return k;
			for (i = 0; i < r; i++) s[k + i] = buf[i];
		}
		k += r;
	}
}
