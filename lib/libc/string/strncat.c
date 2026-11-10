#include <string.h>
char *strncat(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	char *a = d + strlen(d);
	for (; n && *s; n--) *a++ = *s++;
	*a = 0;
	return d;
}
