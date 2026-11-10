#include <stdlib.h>
void *bsearch(const void *key, const void *base, size_t n, size_t w,
              int (*cmp)(const void *, const void *))
{
	const char *b = base;
	while (n) {
		const char *mid = b + (n / 2) * w;
		int c = cmp(key, mid);
		if (!c) return (void *)mid;
		if (c > 0) { b = mid + w; n -= n / 2 + 1; }
		else n /= 2;
	}
	return 0;
}
