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

/* Numbers from wide strings: the leading white space skipped here, the
 * rest (ASCII, as numbers are) converted by the narrow functions. */
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>

/* the ASCII run after the white space, as a narrow string; *skip is the
 * white space's length. Short runs use buf. */
static char *narrow(const wchar_t *s, char *buf, size_t size, size_t *skip)
{
	const wchar_t *p = s, *q;
	char *r;
	size_t n, i;
	while (iswspace((wint_t)*p)) p++;
	*skip = (size_t)(p - s);
	for (q = p; *q > 0 && *q < 0x80; q++);
	n = (size_t)(q - p);
	r = n < size ? buf : malloc(n + 1);
	if (!r) return 0;
	for (i = 0; i < n; i++) r[i] = (char)p[i];
	r[n] = 0;
	return r;
}

#define CONV(T, name, call) \
T name(const wchar_t *__RESTRICT s, wchar_t **__RESTRICT end, int base) \
{ \
	char buf[128], *n, *e; \
	size_t skip; \
	T v; \
	if (!(n = narrow(s, buf, sizeof buf, &skip))) { if (end) *end = (wchar_t *)s; return 0; } \
	v = call(n, &e, base); \
	if (end) *end = (wchar_t *)(e == n ? s : s + skip + (e - n)); \
	if (n != buf) free(n); \
	return v; \
}
CONV(long, wcstol, strtol)
CONV(long long, wcstoll, strtoll)
CONV(unsigned long, wcstoul, strtoul)
CONV(unsigned long long, wcstoull, strtoull)

#define FCONV(T, name, call) \
T name(const wchar_t *__RESTRICT s, wchar_t **__RESTRICT end) \
{ \
	char buf[128], *n, *e; \
	size_t skip; \
	T v; \
	if (!(n = narrow(s, buf, sizeof buf, &skip))) { if (end) *end = (wchar_t *)s; return 0; } \
	v = call(n, &e); \
	if (end) *end = (wchar_t *)(e == n ? s : s + skip + (e - n)); \
	if (n != buf) free(n); \
	return v; \
}
FCONV(float, wcstof, strtof)
FCONV(double, wcstod, strtod)
FCONV(long double, wcstold, strtold)

#include <inttypes.h>
CONV(intmax_t, wcstoimax, strtoimax)
CONV(uintmax_t, wcstoumax, strtoumax)
