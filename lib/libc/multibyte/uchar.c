#include <uchar.h>
#include <wchar.h>
#include <errno.h>
#include "locale_impl.h"
#include "mb_impl.h"

size_t mbrtoc32(char32_t *__RESTRICT c, const char *__RESTRICT s, size_t n, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	wchar_t w;
	size_t r = __mbrtowc_l(&w, s, n, st ? st : &internal, CURRENT_CTYPE);
	if (r <= 4 && c && s) *c = (char32_t)w;
	return r;
}

size_t c32rtomb(char *__RESTRICT s, char32_t c, mbstate_t *__RESTRICT st)
{
	(void)st;
	if (!s) return 1;
	return __wcrtomb_l(s, (wchar_t)c, CURRENT_CTYPE);
}

/* UTF-16: a character beyond the BMP gives its low surrogate on the next
 * call, which consumes nothing (returns -3); the state's second word
 * holds it, marked by bit 31 of the first. */
size_t mbrtoc16(char16_t *__RESTRICT c, const char *__RESTRICT s, size_t n, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	wchar_t w;
	size_t r;
	if (!st) st = &internal;
	if (st->__st[0] & 0x80000000u) {
		if (c) *c = (char16_t)st->__st[1];
		st->__st[0] = st->__st[1] = 0;
		return (size_t)-3;
	}
	r = __mbrtowc_l(&w, s, n, st, CURRENT_CTYPE);
	if (r > 4) return r;
	if ((unsigned)w >= 0x10000) {
		if (c) *c = (char16_t)(0xd7c0 + ((unsigned)w >> 10));
		st->__st[0] = 0x80000000u;
		st->__st[1] = 0xdc00 + ((unsigned)w & 0x3ff);
	} else if (c && s)
		*c = (char16_t)w;
	return r;
}

/* a high surrogate waits in the state for its low surrogate */
size_t c16rtomb(char *__RESTRICT s, char16_t c, mbstate_t *__RESTRICT st)
{
	static mbstate_t internal;
	unsigned hi;
	if (!st) st = &internal;
	if (!s) { st->__st[0] = st->__st[1] = 0; return 1; }
	hi = st->__st[1];
	if (hi) {
		st->__st[1] = 0;
		if (c - 0xdc00u >= 0x400) { errno = EILSEQ; return (size_t)-1; }
		return __wcrtomb_l(s, (wchar_t)(0x10000 + ((hi - 0xd800) << 10) + (c - 0xdc00u)), CURRENT_CTYPE);
	}
	if (c - 0xd800u < 0x400) { st->__st[1] = c; return 0; }
	if (c - 0xdc00u < 0x400) { errno = EILSEQ; return (size_t)-1; }
	return __wcrtomb_l(s, (wchar_t)c, CURRENT_CTYPE);
}
