#include <string.h>
char *strcpy(char *__RESTRICT d, const char *__RESTRICT s)
{
	char *r = d;
	while ((*d++ = *s++));
	return r;
}
