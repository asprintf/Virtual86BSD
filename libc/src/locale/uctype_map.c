/* Case mappings of the built-in C.UTF-8 locale. */
#include <stdint.h>
#include <stddef.h>
#include "locale_impl.h"
#include "uctype_map.h"

static int32_t lookup(const int32_t *runs, size_t nruns, uint32_t c)
{
	size_t lo = 0, hi = nruns, i, k;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if ((uint32_t)runs[4 * m] <= c) lo = m + 1; else hi = m;
	}
	/* runs of every other character can enclose a few others */
	for (i = lo, k = 0; i > 0 && k < 4; i--, k++) {
		const int32_t *r = runs + 4 * (i - 1);
		uint32_t d = c - (uint32_t)r[0];
		if (d < (uint32_t)(r[1] * r[3]) && d % (uint32_t)r[3] == 0) return r[2];
	}
	return 0;
}

uint32_t __uctype_toupper(uint32_t c) { return c + (uint32_t)lookup(u_toupper, sizeof u_toupper / 16, c); }
uint32_t __uctype_tolower(uint32_t c) { return c + (uint32_t)lookup(u_tolower, sizeof u_tolower / 16, c); }
