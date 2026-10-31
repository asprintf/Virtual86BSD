#include <string.h>
int memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *l = a, *r = b;
	for (; n && *l == *r; n--, l++, r++);
	return n ? *l - *r : 0;
}
