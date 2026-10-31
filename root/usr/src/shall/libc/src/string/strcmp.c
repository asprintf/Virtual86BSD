#include <string.h>
int strcmp(const char *l, const char *r)
{
	for (; *l == *r && *l; l++, r++);
	return *(const unsigned char *)l - *(const unsigned char *)r;
}
