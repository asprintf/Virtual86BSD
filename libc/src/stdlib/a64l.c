#include <stdlib.h>
#include <stdint.h>

static const char digits[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

long a64l(const char *s)
{
	uint32_t x = 0;
	int e;
	for (e = 0; e < 36 && *s; e += 6, s++) {
		int d;
		for (d = 0; d < 64 && digits[d] != *s; d++);
		if (d == 64) break;
		x |= (uint32_t)d << e;
	}
	return (long)(int32_t)x;
}

char *l64a(long x0)
{
	static char s[7];
	char *p;
	uint32_t x = (uint32_t)x0;
	for (p = s; x; p++, x >>= 6) *p = digits[x & 63];
	*p = 0;
	return s;
}
