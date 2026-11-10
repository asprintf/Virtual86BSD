#include <string.h>
char *stpncpy(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	size_t k = strnlen(s, n);
	memcpy(d, s, k);
	memset(d + k, 0, n - k);
	return d + k;
}
