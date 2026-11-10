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
#include "locale_impl.h"
#include "mb_impl.h"

size_t mbsnrtowcs(wchar_t *__RESTRICT ws, const char **__RESTRICT src, size_t nms, size_t n,
                  mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	const struct __lcat *ct = CURRENT_CTYPE;
	const char *s = *src, *end = s + nms;
	size_t k = 0, r;
	wchar_t w;
	if (!st) st = &internal;
	for (; !ws || k < n; k++) {
		if (s >= end) break;
		r = __mbrtowc_l(&w, s, (size_t)(end - s), st, ct);
		if (r == (size_t)-1) { if (ws) *src = s; return r; }
		if (r == (size_t)-2) { s = end; break; }   /* the rest is in the state */
		if (ws) ws[k] = w;
		if (!r) { if (ws) *src = 0; return k; }
		s += r;
	}
	if (ws) *src = s;
	return k;
}

size_t wcsnrtombs(char *__RESTRICT s, const wchar_t **__RESTRICT src, size_t nwc, size_t n,
                  mbstate_t *__RESTRICT st)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	const wchar_t *w = *src, *end = w + nwc;
	char buf[8];
	size_t k = 0, r;
	(void)st;
	for (; w < end; w++) {
		if (!*w) {
			if (s) { if (k < n) { s[k] = 0; *src = 0; } else *src = w; }
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
	if (s) *src = w;
	return k;
}
