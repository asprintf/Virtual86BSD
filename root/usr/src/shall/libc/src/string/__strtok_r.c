#include <string.h>
char *__strtok_r(char *__RESTRICT s, const char *__RESTRICT sep, char **__RESTRICT p)
{
	if (!s && !(s = *p)) return 0;
	s += strspn(s, sep);
	if (!*s) return *p = 0;
	*p = s + strcspn(s, sep);
	if (**p) *(*p)++ = 0;
	else *p = 0;
	return s;
}
