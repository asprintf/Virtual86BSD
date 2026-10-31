#include <string.h>
char *strncpy(char *__RESTRICT d, const char *__RESTRICT s, size_t n)
{
	size_t k = 0;
	for (; k < n && s[k]; k++) d[k] = s[k];
	memset(d + k, 0, n - k);
	return d;
}
