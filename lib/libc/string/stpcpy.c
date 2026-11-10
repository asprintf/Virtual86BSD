#include <string.h>
char *stpcpy(char *__RESTRICT d, const char *__RESTRICT s)
{
	while ((*d = *s)) d++, s++;
	return d;
}
