/* Regular expressions in multibyte codesets other than UTF-8 (EUC-JP,
 * SHIFT_JIS, GB18030 ...): pattern and subject are converted to UTF-8,
 * which the engine handles, and match offsets mapped back.  This also
 * keeps trail bytes in the ASCII range (Shift_JIS, Big5, GBK) from being
 * taken for '\\', '[' or '*'. */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "locale_impl.h"
#include "mb_impl.h"
#include "regex_impl.h"
#include "../iconv/iconv_impl.h"

char *__re_to_u8(const char *s, size_t n, const struct __lcat *ct, long **off, size_t *len)
{
	/* at most 4 bytes for each byte: a 1-byte character can become 3 */
	char *u = malloc(n * 4 + 1), *q = u;
	long *o = off ? malloc((n * 4 + 1) * sizeof *o) : 0;
	size_t i = 0, r, k;
	if (!u || (off && !o)) {
		free(u);
		free(o);
		return 0;
	}
	while (i < n) {
		mbstate_t st;
		wchar_t w;
		char b[8];
		memset(&st, 0, sizeof st);
		r = __mbrtowc_l(&w, s + i, n - i, &st, ct);
		/* (GBK's 0x80, the euro sign, is no single-byte character for glibc's
		 * regex: btowc() gives WEOF; "." does not match it) */
		if (r == (size_t)-1 || r == (size_t)-2 || !r || (r == 1 && ct->mbkind == GBK && (unsigned char)s[i] == 0x80) || (k = __wcrtomb_l(b, w, &__c_utf8_ctype)) == (size_t)-1) {
			/* no character: a byte that is none in UTF-8 either (as glibc,
			 * "." does not match it); 0x80-0xbf as they are (they cannot
			 * start a sequence), the others as 0xff */
			unsigned char c = (unsigned char)s[i++];
			if (o) o[q - u] = (long)(i - 1);
			*q++ = (char)(c < 0x80 || (c >= 0x80 && c < 0xc0) ? c : 0xff);
			continue;
		}
		memcpy(q, b, k);
		if (o) {
			size_t j;
			for (j = 0; j < k; j++) o[q - u + (long)j] = (long)i;
		}
		q += k;
		i += r;
	}
	if (o) o[q - u] = (long)n;
	*q = 0;
	if (len) *len = (size_t)(q - u);
	if (off) *off = o;
	return u;
}
