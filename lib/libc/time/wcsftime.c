#include <wchar.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* through strftime(): the format to multibyte, the result back */
size_t wcsftime(wchar_t *__RESTRICT s, size_t max, const wchar_t *__RESTRICT fmt, const struct tm *__RESTRICT tm)
{
	const wchar_t *p = fmt;
	char *f, *b;
	size_t fl = wcsrtombs(0, &p, 0, 0), bl, r, n;
	const char *q;
	if (fl == (size_t)-1 || !max) return 0;
	if (!(f = malloc(fl + 1))) return 0;
	p = fmt;
	wcsrtombs(f, &p, fl + 1, 0);
	bl = max * 4 + fl * 4 + 256;
	if (!(b = malloc(bl))) { free(f); return 0; }
	r = strftime(b, bl, f, tm);
	free(f);
	if (!r && *fmt) { free(b); return 0; }
	q = b;
	n = mbsrtowcs(s, &q, max, 0);
	free(b);
	if (n == (size_t)-1 || n >= max) return 0;
	return n;
}
