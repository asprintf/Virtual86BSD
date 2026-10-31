#include <string.h>
char *__strtok_r(char *__RESTRICT, const char *__RESTRICT, char **__RESTRICT);
char *strtok(char *__RESTRICT s, const char *__RESTRICT sep)
{
	static char *p;
	return __strtok_r(s, sep, &p);
}
