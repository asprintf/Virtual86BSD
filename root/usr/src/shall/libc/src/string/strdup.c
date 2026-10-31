#include <string.h>
#include <stdlib.h>
char *strdup(const char *s)
{
	size_t l = strlen(s) + 1;
	char *d = malloc(l);
	return d ? memcpy(d, s, l) : 0;
}
char *strndup(const char *s, size_t n)
{
	size_t l = strnlen(s, n);
	char *d = malloc(l + 1);
	if (!d) return 0;
	memcpy(d, s, l);
	d[l] = 0;
	return d;
}
