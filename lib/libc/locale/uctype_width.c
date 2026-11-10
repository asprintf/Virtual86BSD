/* Column widths of the built-in C.UTF-8 locale (printable characters). */
#include <stdint.h>
#include <stddef.h>
#include "locale_impl.h"
#include "uctype_width.h"

static int in(const uint32_t *r, size_t n, uint32_t c)
{
	size_t lo = 0, hi = n / 2;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if (c < r[2 * m]) hi = m;
		else if (c > r[2 * m + 1]) lo = m + 1;
		else return 1;
	}
	return 0;
}

int __uctype_width(uint32_t c)
{
	if (in(u_width0, sizeof u_width0 / sizeof *u_width0, c)) return 0;
	if (in(u_width2, sizeof u_width2 / sizeof *u_width2, c)) return 2;
	return 1;
}
