#include <string.h>
size_t strlcpy(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	size_t len = strlen(s);
	if (n) {
		size_t k = len < n ? len : n - 1;
		memcpy(d, s, k);
		d[k] = 0;
	}
	return len;
}
