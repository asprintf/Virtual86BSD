#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

void *reallocarray(void *p, size_t m, size_t n)
{
	if (n && m > SIZE_MAX / n) { errno = ENOMEM; return 0; }
	return realloc(p, m * n);
}
