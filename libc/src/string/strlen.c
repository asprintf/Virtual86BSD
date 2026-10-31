#include <string.h>
#include <stdint.h>
#define ONES ((uint64_t)-1 / 255)
#define HIGHS (ONES * 128)
#define HASZERO(x) (((x) - ONES) & ~(x) & HIGHS)
size_t strlen(const char *s)
{
	const char *a = s;
	const uint64_t *w;
	for (; (uintptr_t)s & 7; s++) if (!*s) return (size_t)(s - a);
	/* Aligned word reads never cross a page boundary. */
	for (w = (const void *)s; !HASZERO(*w); w++);
	for (s = (const void *)w; *s; s++);
	return (size_t)(s - a);
}
