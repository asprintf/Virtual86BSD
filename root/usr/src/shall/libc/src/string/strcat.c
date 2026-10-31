#include <string.h>
char *strcat(char *__RESTRICT d, const char *__RESTRICT s)
{
	strcpy(d + strlen(d), s);
	return d;
}
