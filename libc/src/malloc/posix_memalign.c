#include <stdlib.h>
#include <errno.h>

void *__alloc_aligned(size_t, size_t);

int posix_memalign(void **res, size_t align, size_t n)
{
	void *p;
	if (align < sizeof(void *) || (align & (align - 1))) return EINVAL;
	p = __alloc_aligned(align, n);
	if (!p) return ENOMEM;
	*res = p;
	return 0;
}
