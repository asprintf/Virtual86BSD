#include <string.h>
void *__memrchr(const void *src, int c, size_t n)
{
	const unsigned char *s = (const unsigned char *)src + n;
	unsigned char ch = (unsigned char)c;
	while (n--) if (*--s == ch) return (void *)s;
	return 0;
}
