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

/* Eras and alternative digits of LC_TIME, shared by strftime() and
 * strptime(). */
#include <string.h>
#include <wchar.h>
#include "era.h"

unsigned __era_count(const struct __lcat *t)
{
	uint32_t n;
	if (t->n <= T_ERA_ENTRIES || !t->elem[T_ERA_N] || !t->elem[T_ERA_ENTRIES]) return 0;
	memcpy(&n, t->elem[T_ERA_N], 4);
	return n;
}

static int le(const int32_t *a, const int32_t *b)
{
	return a[0] < b[0] || (a[0] == b[0] && (a[1] < b[1] || (a[1] == b[1] && a[2] <= b[2])));
}

/* entry i (< __era_count): direction, offset, start[3], stop[3] as int32;
 * the name and the format; aligned to 4; their wide forms */
void __era_get(const struct __lcat *t, unsigned i, struct __era *e)
{
	const char *p = t->elem[T_ERA_ENTRIES], *base;
	int32_t dir, off;
	for (;;) {
		base = p;
		memcpy(&dir, p, 4);
		memcpy(&off, p + 4, 4);
		memcpy(e->start, p + 8, 12);
		memcpy(e->stop, p + 20, 12);
		p += 32;
		e->name = p;
		p += strlen(p) + 1;
		e->format = p;
		p += strlen(p) + 1;
		if (!i--) break;
		p += 3 - ((p - base + 3) & 3);
		p += 4 * (wcslen((const wchar_t *)(const void *)p) + 1);
		p += 4 * (wcslen((const wchar_t *)(const void *)p) + 1);
	}
	e->offset = off;
	e->dir = (le(e->start, e->stop) ? 1 : -1) * (dir == '+' ? 1 : -1);
}

int __era_has(const struct __era *e, const int32_t d[3])
{
	return (le(e->start, d) && le(d, e->stop)) || (le(e->stop, d) && le(d, e->start));
}

/* as glibc: the years counted from the offset, in the era's direction, up
 * to the length of the era */
int __era_year_ok(const struct __era *e, long long y)
{
	long long d = (y - e->offset) * e->dir;
	return d >= 0 && d <= ((long long)e->stop[0] - e->start[0]) * e->dir;
}

/* the list of alternative digits: strings separated by nulls, ended by the
 * next element (ERA_D_T_FMT) */
static const char *alt_list(const struct __lcat *t, const char **lim)
{
	const char *p;
	if (t->n <= T_ALTDIGITS || !(p = t->elem[T_ALTDIGITS]) || !*p) return 0;
	*lim = t->n > T_ERA_DTFMT && t->elem[T_ERA_DTFMT] > p ? t->elem[T_ERA_DTFMT] : 0;
	return p;
}

const char *__alt_digit(const struct __lcat *t, long long v)
{
	const char *lim, *p = alt_list(t, &lim);
	if (!p || v < 0 || v >= 100) return 0;
	for (; v; v--) {
		p += strlen(p) + 1;
		if (lim && p >= lim) return 0;
	}
	return *p ? p : 0;
}

int __alt_digit_scan(const struct __lcat *t, const char **s)
{
	const char *lim, *p = alt_list(t, &lim);
	size_t best = 0, l;
	int v, r = -1;
	if (!p) return -1;
	for (v = 0; v < 100 && (!lim || p < lim) && *p; v++, p += l + 1) {
		l = strlen(p);
		if (l > best && !strncmp(*s, p, l)) { best = l; r = v; }
	}
	*s += best;
	return r;
}
