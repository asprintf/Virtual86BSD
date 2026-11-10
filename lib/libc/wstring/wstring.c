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

/* The wide string functions of ISO C. */
#include <wchar.h>
#include "locale_impl.h"

wchar_t *wcscpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s)
{
	wchar_t *r = d;
	while ((*d++ = *s++));
	return r;
}

wchar_t *wcsncpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	wchar_t *r = d;
	for (; n && *s; n--) *d++ = *s++;
	for (; n; n--) *d++ = 0;
	return r;
}

wchar_t *wcscat(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s)
{
	wcscpy(d + wcslen(d), s);
	return d;
}

wchar_t *wcsncat(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	wchar_t *r = d;
	d += wcslen(d);
	for (; n && *s; n--) *d++ = *s++;
	*d = 0;
	return r;
}

size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p) p++;
	return (size_t)(p - s);
}

int wcscmp(const wchar_t *a, const wchar_t *b)
{
	for (; *a == *b && *a; a++, b++);
	return *a < *b ? -1 : *a > *b;
}

int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n)
{
	for (; n && *a == *b && *a; a++, b++, n--);
	return n ? (*a < *b ? -1 : *a > *b) : 0;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
	for (; *s != c; s++) if (!*s) return 0;
	return (wchar_t *)s;
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c)
{
	const wchar_t *r = 0;
	for (;; s++) {
		if (*s == c) r = s;
		if (!*s) return (wchar_t *)r;
	}
}

size_t wcsspn(const wchar_t *s, const wchar_t *set)
{
	const wchar_t *p = s;
	while (*p && wcschr(set, *p)) p++;
	return (size_t)(p - s);
}

size_t wcscspn(const wchar_t *s, const wchar_t *set)
{
	const wchar_t *p = s;
	while (*p && !wcschr(set, *p)) p++;
	return (size_t)(p - s);
}

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set)
{
	s += wcscspn(s, set);
	return *s ? (wchar_t *)s : 0;
}

wchar_t *wcsstr(const wchar_t *__RESTRICT h, const wchar_t *__RESTRICT n)
{
	size_t l = wcslen(n);
	if (!l) return (wchar_t *)h;
	for (; (h = wcschr(h, *n)); h++)
		if (!wcsncmp(h, n, l)) return (wchar_t *)h;
	return 0;
}

wchar_t *wcstok(wchar_t *__RESTRICT s, const wchar_t *__RESTRICT sep, wchar_t **__RESTRICT p)
{
	if (!s && !(s = *p)) return 0;
	s += wcsspn(s, sep);
	if (!*s) return *p = 0;
	*p = s + wcscspn(s, sep);
	if (**p) *(*p)++ = 0;
	else *p = 0;
	return s;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
	for (; n; n--, s++) if (*s == c) return (wchar_t *)s;
	return 0;
}

int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n)
{
	for (; n; n--, a++, b++) if (*a != *b) return *a < *b ? -1 : 1;
	return 0;
}

wchar_t *wmemcpy(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	wchar_t *r = d;
	while (n--) *d++ = *s++;
	return r;
}

wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n)
{
	wchar_t *r = d;
	if ((size_t)(d - s) >= n) while (n--) *d++ = *s++;
	else while (n--) d[n] = s[n];
	return r;
}

wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n)
{
	wchar_t *r = d;
	while (n--) *d++ = c;
	return r;
}

/* collation: code point order (see STATUS.md) */
int wcscoll(const wchar_t *a, const wchar_t *b) { return __coll_cmp(CURRENT_LOCALE->cat[LC_COLLATE], a, b, 1); }

size_t wcsxfrm(wchar_t *__RESTRICT d, const wchar_t *__RESTRICT s, size_t n)
{
	return __coll_xfrm(CURRENT_LOCALE->cat[LC_COLLATE], d, s, n, 1);
}
