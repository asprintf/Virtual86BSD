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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "locale_impl.h"
#include "lock.h"

static const char *const envnames[6] = {
	"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES",
};

static volatile int lock;
static char allname[6 * (LOCNAME_MAX + 14)];

/* the name for category cat from the environment (setlocale(cat, "")) */
static const char *fromenv(int cat)
{
	const char *s = getenv("LC_ALL");
	if (!s || !*s) s = getenv(envnames[cat]);
	if (!s || !*s) s = getenv("LANG");
	if (!s || !*s) s = "C";
	return s;
}

static char *query(int cat)
{
	int i, same = 1;
	char *p;
	if (cat != LC_ALL) return (char *)__global_locale.cat[cat]->name;
	for (i = 1; i < 6; i++)
		if (strcmp(__global_locale.cat[i]->name, __global_locale.cat[0]->name)) same = 0;
	if (same) return (char *)__global_locale.cat[0]->name;
	p = allname;
	for (i = 0; i < 6; i++) {
		size_t a = strlen(envnames[i]), b = strlen(__global_locale.cat[i]->name);
		memcpy(p, envnames[i], a); p += a;
		*p++ = '=';
		memcpy(p, __global_locale.cat[i]->name, b); p += b;
		*p++ = i < 5 ? ';' : 0;
	}
	return allname;
}

char *setlocale(int cat, const char *name)
{
	const struct __lcat *c[6];
	char *r;
	int i;
	if ((unsigned)cat > LC_ALL) return 0;
	LOCK(&lock);
	if (!name) { r = query(cat); UNLOCK(&lock); return r; }
	if (cat != LC_ALL) {
		const char *n = *name ? name : fromenv(cat);
		const struct __lcat *x = __get_lcat(cat, n);
		if (x) __global_locale.cat[cat] = x;
		r = x ? (char *)x->name : 0;
		UNLOCK(&lock);
		return r;
	}
	if (strchr(name, ';')) {
		/* a composite name, as returned above */
		const char *p = name;
		for (i = 0; i < 6; i++) c[i] = 0;
		while (*p) {
			const char *eq = strchr(p, '='), *e;
			char one[LOCNAME_MAX + 1];
			int k;
			if (!eq) goto fail;
			e = strchr(eq, ';');
			if (!e) e = eq + strlen(eq);
			for (k = 0; k < 6; k++)
				if (strlen(envnames[k]) == (size_t)(eq - p) && !memcmp(envnames[k], p, (size_t)(eq - p))) break;
			if (k == 6 || (size_t)(e - eq - 1) > LOCNAME_MAX) goto fail;
			memcpy(one, eq + 1, (size_t)(e - eq - 1));
			one[e - eq - 1] = 0;
			if (!(c[k] = __get_lcat(k, one))) goto fail;
			p = *e ? e + 1 : e;
		}
		for (i = 0; i < 6; i++) if (!c[i]) goto fail;
	} else {
		for (i = 0; i < 6; i++)
			if (!(c[i] = __get_lcat(i, *name ? name : fromenv(i)))) goto fail;
	}
	for (i = 0; i < 6; i++) __global_locale.cat[i] = c[i];
	r = query(LC_ALL);
	UNLOCK(&lock);
	return r;
fail:
	UNLOCK(&lock);
	return 0;
}
