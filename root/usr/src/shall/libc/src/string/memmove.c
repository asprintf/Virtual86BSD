#include <string.h>
#include <stdint.h>
void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	if (d == s || !n) return dest;
	if ((uintptr_t)d - (uintptr_t)s >= n) return memcpy(dest, src, n); /* no forward overlap */
	/* d > s and overlapping: copy backwards */
	d += n; s += n;
	if (((uintptr_t)d & 7) == ((uintptr_t)s & 7)) {
		while ((uintptr_t)d & 7) { if (!n--) return dest; *--d = *--s; }
		for (; n >= 8; n -= 8) { d -= 8; s -= 8; *(uint64_t *)(void *)d = *(const uint64_t *)(const void *)s; }
	}
	while (n--) *--d = *--s;
	return dest;
}
