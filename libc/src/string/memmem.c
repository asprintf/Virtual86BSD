/* Two-way-free but linear-expected: Rabin-Karp style rolling compare is
 * overkill here; use a simple skip loop with a first-byte memchr. */
#include <string.h>
void *memmem(const void *h0, size_t hl, const void *n0, size_t nl)
{
	const unsigned char *h = h0, *n = n0, *end;
	if (!nl) return (void *)h;
	if (hl < nl) return 0;
	end = h + (hl - nl);
	for (;;) {
		h = memchr(h, n[0], (size_t)(end - h) + 1);
		if (!h) return 0;
		if (!memcmp(h + 1, n + 1, nl - 1)) return (void *)h;
		if (h++ == end) return 0;
	}
}
