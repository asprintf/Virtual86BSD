#include "libm.h"

/* the payload of "NAN(n-char-sequence)": the digits as strtoull reads
 * them (decimal, octal or hexadecimal), or 0 */
static uint64_t payload(const char *s)
{
	uint64_t v = 0;
	unsigned base = 10, d;
	if (*s == '0') {
		base = 8;
		s++;
		if (*s == 'x' || *s == 'X') {
			base = 16;
			s++;
		}
	}
	for (; *s; s++) {
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (*s >= 'a' && *s <= 'f')
			d = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F')
			d = *s - 'A' + 10;
		else
			return 0;
		if (d >= base)
			return 0;
		v = v * base + d;
	}
	return v;
}

double nan(const char *s)
{
	union __dbits u;
	u.i = 0x7ff8000000000000ULL | (payload(s) & ((1ULL << 51) - 1));
	return u.f;
}

float nanf(const char *s)
{
	union __fbits u;
	u.i = 0x7fc00000 | (uint32_t)(payload(s) & ((1u << 22) - 1));
	return u.f;
}

long double nanl(const char *s)
{
	union __ldbits u;
	u.i.m = 0xc000000000000000ULL | (payload(s) & ((1ULL << 62) - 1));
	u.i.se = 0x7fff;
	return u.f;
}
