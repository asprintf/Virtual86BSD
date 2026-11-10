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

/* The wide string functions of POSIX. */
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <locale.h>
#include "locale_impl.h"

wchar_t *wcpcpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s)
{
	while ((*d = *s++)) d++;
	return d;
}

wchar_t *wcpncpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	for (; n && *s; n--) *d++ = *s++;
	wmemset(d, 0, n);
	return d;
}

size_t wcsnlen(const wchar_t *s, size_t n)
{
	size_t k = 0;
	while (k < n && s[k]) k++;
	return k;
}

wchar_t *wcsdup(const wchar_t *s)
{
	size_t l = wcslen(s) + 1;
	wchar_t *d = malloc(l * sizeof *d);
	return d ? wmemcpy(d, s, l) : 0;
}

size_t wcslcpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	size_t l = wcslen(s);
	if (n) {
		size_t k = l < n - 1 ? l : n - 1;
		wmemcpy(d, s, k);
		d[k] = 0;
	}
	return l;
}

size_t wcslcat(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	size_t dl = wcsnlen(d, n);
	if (dl == n) return n + wcslen(s);
	return dl + wcslcpy(d + dl, s, n - dl);
}

static const struct __lcat *ct(locale_t l)
{
	return (l == LC_GLOBAL_LOCALE ? &__global_locale : l)->cat[LC_CTYPE];
}

int wcsncasecmp_l(const wchar_t *a, const wchar_t *b, size_t n, locale_t l)
{
	const struct __lcat *c = ct(l);
	for (; n; a++, b++, n--) {
		unsigned x = __towctrans_l((unsigned)*a, 2, c), y = __towctrans_l((unsigned)*b, 2, c);
		if (x != y) return x < y ? -1 : 1;
		if (!x) break;
	}
	return 0;
}

int wcscasecmp_l(const wchar_t *a, const wchar_t *b, locale_t l) { return wcsncasecmp_l(a, b, (size_t)-1, l); }
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n) { return wcsncasecmp_l(a, b, n, CURRENT_LOCALE); }
int wcscasecmp(const wchar_t *a, const wchar_t *b) { return wcsncasecmp_l(a, b, (size_t)-1, CURRENT_LOCALE); }
int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t l) { return __coll_cmp(COLL_OF(l), a, b, 1); }
size_t wcsxfrm_l(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n, locale_t l) { return __coll_xfrm(COLL_OF(l), d, s, n, 1); }
