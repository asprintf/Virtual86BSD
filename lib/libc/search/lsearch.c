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

/* lsearch, lfind, insque, remque */
#include <search.h>
#include <string.h>

void *lfind(const void *key, const void *base, size_t *nelp, size_t width, int (*cmp)(const void *, const void *))
{
	const char *p = base;
	size_t i, n = *nelp;
	for (i = 0; i < n; i++, p += width)
		if (!cmp(key, p))
			return (void *)p;
	return 0;
}

void *lsearch(const void *key, void *base, size_t *nelp, size_t width, int (*cmp)(const void *, const void *))
{
	void *p = lfind(key, base, nelp, width, cmp);
	if (p)
		return p;
	p = (char *)base + width * *nelp;
	memcpy(p, key, width);
	++*nelp;
	return p;
}

struct q {
	struct q *next, *prev;
};

void insque(void *elem, void *pred)
{
	struct q *e = elem, *p = pred;
	if (!p) {
		e->next = e->prev = 0;
		return;
	}
	e->next = p->next;
	e->prev = p;
	p->next = e;
	if (e->next)
		e->next->prev = e;
}

void remque(void *elem)
{
	struct q *e = elem;
	if (e->next)
		e->next->prev = e->prev;
	if (e->prev)
		e->prev->next = e->next;
}
