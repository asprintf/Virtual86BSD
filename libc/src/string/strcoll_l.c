#include <string.h>
#include "locale_impl.h"

int strcoll_l(const char *l, const char *r, locale_t loc)
{
	return __coll_cmp(COLL_OF(loc), l, r, 0);
}

size_t strxfrm_l(char *__RESTRICT d, const char *__RESTRICT s, size_t n, locale_t loc)
{
	return __coll_xfrm(COLL_OF(loc), d, s, n, 0);
}
