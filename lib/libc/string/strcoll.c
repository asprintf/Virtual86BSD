/* Collation by the locale's LC_COLLATE (src/locale/collate.c). */
#include <string.h>
#include "locale_impl.h"

int strcoll(const char *l, const char *r)
{
	return __coll_cmp(CURRENT_LOCALE->cat[LC_COLLATE], l, r, 0);
}

size_t strxfrm(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	return __coll_xfrm(CURRENT_LOCALE->cat[LC_COLLATE], d, s, n, 0);
}
