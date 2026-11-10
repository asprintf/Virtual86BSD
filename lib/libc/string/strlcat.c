#include <string.h>
size_t strlcat(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	size_t l = strnlen(d, n);
	if (l == n) return l + strlen(s);
	return l + strlcpy(d + l, s, n - l);
}
