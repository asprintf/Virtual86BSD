#include <stdlib.h>
void __qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *);
void qsort_r(void *base, size_t n, size_t w, int (*cmp)(const void *, const void *, void *), void *arg)
{
	__qsort_r(base, n, w, cmp, arg);
}
