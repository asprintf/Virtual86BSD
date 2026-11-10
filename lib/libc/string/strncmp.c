#include <string.h>
int strncmp(const char *l, const char *r, size_t n)
{
	if (!n) return 0;
	for (; --n && *l == *r && *l; l++, r++);
	return *(const unsigned char *)l - *(const unsigned char *)r;
}
