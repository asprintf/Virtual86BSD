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
