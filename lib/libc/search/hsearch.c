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

/* hcreate, hsearch, hdestroy: one table per process, open addressing
 * with linear probing on a power-of-two table that doubles when it is
 * three quarters full (so ENTER fails only when memory runs out; nel is
 * the initial size estimate).  Errors as glibc: ENOMEM, ESRCH. */
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static ENTRY *tab;
static size_t mask, used;

static size_t hash(const char *s)
{
	size_t h = 5381;
	while (*s)
		h = h * 33 ^ (unsigned char)*s++;
	return h;
}

static ENTRY *slot(ENTRY *t, size_t m, const char *key)
{
	size_t i = hash(key) & m;
	while (t[i].key && strcmp(t[i].key, key))
		i = (i + 1) & m;
	return &t[i];
}

static int resize(size_t n)
{
	size_t sz = 8, i;
	ENTRY *t;
	while (sz < n + n / 3 + 1 && sz < (size_t)-1 / 4 / sizeof *t)
		sz *= 2;
	if (sz < n || !(t = calloc(sz, sizeof *t)))
		return 0;
	if (tab) {
		for (i = 0; i <= mask; i++)
			if (tab[i].key)
				*slot(t, sz - 1, tab[i].key) = tab[i];
		free(tab);
	}
	tab = t;
	mask = sz - 1;
	return 1;
}

int hcreate(size_t nel)
{
	if (tab)
		return 0;
	used = 0;
	if (!resize(nel)) {
		errno = ENOMEM;
		return 0;
	}
	return 1;
}

void hdestroy(void)
{
	free(tab);
	tab = 0;
	mask = used = 0;
}

ENTRY *hsearch(ENTRY item, ACTION action)
{
	ENTRY *e;
	if (!tab) {
		errno = action == ENTER ? ENOMEM : ESRCH;
		return 0;
	}
	e = slot(tab, mask, item.key);
	if (e->key)
		return e;
	if (action == FIND) {
		errno = ESRCH;
		return 0;
	}
	if ((used + 1) * 4 > (mask + 1) * 3) {
		if (!resize(used + 1)) {
			errno = ENOMEM;
			return 0;
		}
		e = slot(tab, mask, item.key);
	}
	*e = item;
	used++;
	return e;
}
