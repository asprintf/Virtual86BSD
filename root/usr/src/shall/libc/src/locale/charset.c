/* Single-byte codesets: their bytes 0x80-0xff (the lower half is ASCII). */
#include <string.h>
#include "locale_impl.h"
#include "charsets_data.h"

/* compare without case and punctuation */
static int same(const char *a, const char *b)
{
	for (;;) {
		int x, y;
		while (*a && !((*a | 32) >= 'a' && (*a | 32) <= 'z') && !(*a >= '0' && *a <= '9')) a++;
		while (*b && !((*b | 32) >= 'a' && (*b | 32) <= 'z') && !(*b >= '0' && *b <= '9')) b++;
		x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
		y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
		if (x != y) return 0;
		if (!x) return 1;
		a++; b++;
	}
}

const struct __charset *__charset_find(const char *name)
{
	unsigned i;
	for (i = 0; i < sizeof charsets / sizeof *charsets; i++)
		if (same(name, charsets[i].name)) return &charsets[i];
	for (i = 0; i < sizeof aliases / sizeof *aliases; i++)
		if (same(name, aliases[i].alias)) return &charsets[aliases[i].set];
	return 0;
}

int __charset_from_wc(const struct __charset *cs, uint32_t wc)
{
	unsigned i;
	if (wc < 0x80) return (int)wc;
	if (wc > 0xffff) return -1;
	for (i = 0; i < 128; i++) if (cs->hi[i] == wc) return (int)(0x80 + i);
	return -1;
}
