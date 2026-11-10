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
#include <errno.h>
#include "locale_impl.h"

locale_t newlocale(int mask, const char *name, locale_t base)
{
	struct __locale_struct t, *r;
	int i;
	if (!name || (mask & ~LC_ALL_MASK)) { errno = EINVAL; return 0; }
	if (base == LC_GLOBAL_LOCALE) base = 0;
	t = base ? *base : __c_locale_obj;
	for (i = 0; i < 6; i++) {
		const char *n;
		if (!(mask & 1 << i)) continue;
		n = name;
		if (!*n) {
			n = getenv("LC_ALL");
			if (!n || !*n) {
				static const char *const env[6] = { "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
					"LC_MONETARY", "LC_MESSAGES" };
				n = getenv(env[i]);
			}
			if (!n || !*n) n = getenv("LANG");
			if (!n || !*n) n = "C";
		}
		if (!(t.cat[i] = __get_lcat(i, n))) { errno = ENOENT; return 0; }
	}
	if (base) { *base = t; return base; }
	if (!(r = malloc(sizeof *r))) return 0;
	*r = t;
	return r;
}

locale_t duplocale(locale_t l)
{
	struct __locale_struct *r = malloc(sizeof *r);
	if (!r) return 0;
	*r = l == LC_GLOBAL_LOCALE ? __global_locale : *l;
	return r;
}

void freelocale(locale_t l)
{
	if (l && l != LC_GLOBAL_LOCALE && l != &__global_locale) free(l);
}

locale_t uselocale(locale_t l)
{
	struct __pthread *self = __pthread_self();
	locale_t old = self->locale ? (locale_t)self->locale : LC_GLOBAL_LOCALE;
	if (l) self->locale = l == LC_GLOBAL_LOCALE ? 0 : l;
	return old;
}

const char *getlocalename_l(int cat, locale_t l)
{
	if ((unsigned)cat >= LC_ALL) return 0;
	if (l == LC_GLOBAL_LOCALE) l = &__global_locale;
	return l->cat[cat]->name;
}
