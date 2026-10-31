#include <strings.h>
#include <ctype.h>
int strcasecmp(const char *_l, const char *_r)
{
	const unsigned char *l = (const void *)_l, *r = (const void *)_r;
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}
int strcasecmp_l(const char *l, const char *r, locale_t loc) { (void)loc; return strcasecmp(l, r); }
int strncasecmp(const char *_l, const char *_r, size_t n)
{
	const unsigned char *l = (const void *)_l, *r = (const void *)_r;
	if (!n--) return 0;
	for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r)); l++, r++, n--);
	return tolower(*l) - tolower(*r);
}
int strncasecmp_l(const char *l, const char *r, size_t n, locale_t loc) { (void)loc; return strncasecmp(l, r, n); }
