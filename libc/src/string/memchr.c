#include <string.h>
void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	unsigned char ch = (unsigned char)c;
	for (; n; n--, s++) if (*s == ch) return (void *)s;
	return 0;
}
