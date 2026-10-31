/* Internal: the codecs of iconv, also the multibyte codesets of locales
 * (EUC-JP, SHIFT_JIS, EUC-CN, GBK, GB18030, EUC-KR, BIG5). */
#ifndef _ICONV_IMPL_H
#define _ICONV_IMPL_H
#include <stddef.h>
#include <stdint.h>

struct __charset;

enum {
	UTF8, UTF16, UTF16LE, UTF16BE, UTF32, UTF32LE, UTF32BE, UCS2, UCS2LE, UCS2BE,
	UCS4, UCS4LE, WCHART, ASCII, SB, EUCJP, SJIS, ISO2022JP, EUCCN, GBK, GB18030, EUCKR, BIG5
};

struct conv {
	int from, to;
	const struct __charset *fcs, *tcs;
	int translit, ignore;
	/* states */
	int in_le;          /* UTF-16/UTF-32/UCS-2 input: -1 not yet known */
	int in_set;         /* ISO-2022-JP input: 0 ASCII, 1 JIS-Roman, 2 JIS X 0208 */
	int out_bom;        /* the byte order mark is written */
	int out_set;        /* ISO-2022-JP output */
};

/* the character at s[0..n): its length, 0 if incomplete, -1 if invalid;
 * -2/-3 for a byte order mark / an escape sequence (no character) */
long __iconv_decode(struct conv *c, const unsigned char *s, size_t n, uint32_t *wc);
/* wc into o[0..n): the length, -1 if the codeset lacks it, -2 if no room */
long __iconv_encode(struct conv *c, uint32_t wc, unsigned char *o, size_t n);

/* the multibyte codeset of a locale (stateless; not UTF-8): its kind, or -1 */
int __mbcs_kind(const char *codeset);
long __mbcs_decode(int kind, const unsigned char *s, size_t n, uint32_t *wc);
long __mbcs_encode(int kind, uint32_t wc, unsigned char *o);
int __mbcs_waits(int kind, unsigned byte);

#endif
