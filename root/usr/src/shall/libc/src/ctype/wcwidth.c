#include <wchar.h>
#include "locale_impl.h"

static int width(unsigned c, const struct __lcat *ct)
{
	int w;
	if (!c) return 0;
	if (ct->wwidth) {
		w = __tab_byte(ct->wwidth, c);
		return w == 0xff ? -1 : w;
	}
	if (c < 0x80) return c - 0x20 < 0x5f ? 1 : -1;
	if (ct->cs != CS_UTF8 || !__uctype_class(W_PRINT, c)) return -1;
	return __uctype_width(c);
}

int wcwidth(wchar_t c)
{
	return width((unsigned)c, CURRENT_CTYPE);
}

int wcswidth(const wchar_t *s, size_t n)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	int k = 0, w;
	for (; n && *s; s++, n--) {
		if ((w = width((unsigned)*s, ct)) < 0) return -1;
		k += w;
	}
	return k;
}
