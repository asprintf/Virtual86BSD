#include <string.h>
void *memccpy(void *__RESTRICT dest, const void *__RESTRICT src, int c, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	unsigned char ch = (unsigned char)c;
	for (; n; n--) if ((*d++ = *s++) == ch) return d;
	return 0;
}
