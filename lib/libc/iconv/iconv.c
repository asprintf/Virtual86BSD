/* iconv: conversion between codesets through Unicode, one character at
 * a time.
 *
 * Codesets: UTF-8; UTF-16, UTF-32, UCS-2, UCS-4 (with BE/LE variants; the
 * forms without one read a byte order mark, else little-endian, and
 * UTF-16/UTF-32 write one, as glibc does); WCHAR_T; ASCII; the
 * single-byte codesets of the locale data (ISO-8859-x, KOI8, CP125x ...);
 * EUC-JP, SHIFT_JIS, ISO-2022-JP, EUC-CN (GB2312), GBK, GB18030, EUC-KR,
 * BIG5.
 *
 * A character the output codeset lacks is an error (EILSEQ), as in glibc,
 * unless the output codeset is suffixed with //TRANSLIT (the locale's
 * transliterations, else '?') or //IGNORE (left out; the call then fails
 * with EILSEQ at its end, having converted the rest). "" names the
 * codeset of the locale. */
#include <iconv.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "locale_impl.h"
#include "iconv_impl.h"

static const struct { const char *name; int kind; } names[] = {
	{ "UTF-8", UTF8 }, { "UTF8", UTF8 },
	{ "UTF-16", UTF16 }, { "UTF-16LE", UTF16LE }, { "UTF-16BE", UTF16BE },
	{ "UTF-32", UTF32 }, { "UTF-32LE", UTF32LE }, { "UTF-32BE", UTF32BE },
	{ "UCS-2", UCS2 }, { "ISO-10646-UCS-2", UCS2 }, { "UCS-2LE", UCS2LE }, { "UNICODELITTLE", UCS2LE },
	{ "UCS-2BE", UCS2BE }, { "UNICODEBIG", UCS2BE },
	{ "UCS-4", UCS4 }, { "ISO-10646-UCS-4", UCS4 }, { "UCS-4BE", UCS4 }, { "UCS-4LE", UCS4LE },
	{ "WCHAR_T", WCHART }, { "INTERNAL", WCHART },
	{ "ASCII", ASCII }, { "US-ASCII", ASCII }, { "ANSI_X3.4-1968", ASCII }, { "ISO646-US", ASCII },
	{ "646", ASCII }, { "US", ASCII }, { "CP367", ASCII }, { "IBM367", ASCII }, { "ISO-IR-6", ASCII },
	{ "EUC-JP", EUCJP }, { "EUCJP", EUCJP },
	{ "SHIFT_JIS", SJIS }, { "SHIFT-JIS", SJIS }, { "SJIS", SJIS }, { "MS_KANJI", SJIS }, { "CSSHIFTJIS", SJIS },
	{ "ISO-2022-JP", ISO2022JP }, { "CSISO2022JP", ISO2022JP }, { "ISO2022JP", ISO2022JP },
	{ "EUC-CN", EUCCN }, { "EUCCN", EUCCN }, { "GB2312", EUCCN }, { "CSGB2312", EUCCN },
	{ "GBK", GBK }, { "CP936", GBK }, { "MS936", GBK }, { "WINDOWS-936", GBK },
	{ "GB18030", GB18030 },
	{ "EUC-KR", EUCKR }, { "EUCKR", EUCKR }, { "CSEUCKR", EUCKR },
	{ "BIG5", BIG5 }, { "BIG-5", BIG5 }, { "BIG-FIVE", BIG5 }, { "CN-BIG5", BIG5 }, { "CSBIG5", BIG5 },
};

/* names compare without case, '-' and '_' */
static int same(const char *a, const char *b, size_t bl)
{
	const char *be = b + bl;
	for (;;) {
		int x, y;
		while (*a == '-' || *a == '_') a++;
		while (b < be && (*b == '-' || *b == '_')) b++;
		x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
		y = b < be ? (*b >= 'a' && *b <= 'z' ? *b - 32 : *b) : 0;
		if (x != y) return 0;
		if (!x) return 1;
		a++; b++;
	}
}

static int find(const char *name, size_t l, const struct __charset **cs)
{
	unsigned i;
	char buf[64];
	if (!l) {
		/* the locale's codeset */
		const struct __lcat *ct = CURRENT_CTYPE;
		if (ct->cs == CS_UTF8) return UTF8;
		if (ct->cs == CS_8BIT) { *cs = ct->charset; return SB; }
		if (ct->cs == CS_MB) return ct->mbkind;
		return ASCII;
	}
	for (i = 0; i < sizeof names / sizeof *names; i++)
		if (same(names[i].name, name, l)) return names[i].kind;
	if (l < sizeof buf) {
		memcpy(buf, name, l);
		buf[l] = 0;
		if ((*cs = __charset_find(buf))) return SB;
	}
	return -1;
}

iconv_t iconv_open(const char *to, const char *from)
{
	struct conv *c;
	const char *sl;
	size_t tl, fl;
	const struct __charset *fcs = 0, *tcs = 0;
	int f, t, translit = 0, ignore = 0;
	/* suffixes: //TRANSLIT, //IGNORE (on the output side; ignored on the input side) */
	fl = (sl = strstr(from, "//")) ? (size_t)(sl - from) : strlen(from);
	tl = (sl = strstr(to, "//")) ? (size_t)(sl - to) : strlen(to);
	if (sl) {
		const char *p = sl;
		while (*p) {
			const char *q;
			while (*p == '/' || *p == ',') p++;
			q = p;
			while (*q && *q != '/' && *q != ',') q++;
			if (same("TRANSLIT", p, (size_t)(q - p))) translit = 1;
			else if (same("IGNORE", p, (size_t)(q - p))) ignore = 1;
			p = q;
		}
	}
	f = find(from, fl, &fcs);
	t = find(to, tl, &tcs);
	if (f < 0 || t < 0) { errno = EINVAL; return (iconv_t)-1; }
	if (!(c = calloc(1, sizeof *c))) return (iconv_t)-1;
	c->from = f; c->to = t; c->fcs = fcs; c->tcs = tcs;
	c->translit = translit; c->ignore = ignore;
	c->in_le = -1;
	return c;
}

int iconv_close(iconv_t cd)
{
	free(cd);
	return 0;
}

/* ---- transliteration: the locale's table (glibc's format), else '?' ---- */

static long translit(struct conv *c, uint32_t wc, unsigned char *o, size_t n)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	if (ct->n > 65 && ct->elem[61]) {
		uint32_t size, lo = 0, hi;
		const uint32_t *fidx = (const void *)ct->elem[62], *ftab = (const void *)ct->elem[63];
		const uint32_t *tidx = (const void *)ct->elem[64], *ttab = (const void *)ct->elem[65];
		memcpy(&size, ct->elem[61], 4);
		hi = size;
		while (lo < hi) {
			uint32_t m = (lo + hi) / 2;
			const uint32_t *f = ftab + fidx[m];
			if (f[0] == wc && !f[1]) {
				/* the alternatives, each null-terminated, until an empty one */
				const uint32_t *t = ttab + tidx[m];
				while (*t) {
					struct conv save = *c;
					size_t k = 0;
					long r = 0;
					for (; *t; t++) {
						if ((r = __iconv_encode(c, *t, o + k, n - k)) < 0) break;
						k += (size_t)r;
					}
					if (r >= 0) return (long)k;
					*c = save;
					if (r == -2) return -2;
					while (*t) t++;
					t++;
				}
				break;
			}
			if (f[0] < wc || (f[0] == wc && f[1])) lo = m + 1;
			else hi = m;
		}
	}
	return __iconv_encode(c, '?', o, n);
}

size_t iconv(iconv_t cd, char **__RESTRICT in, size_t *__RESTRICT inleft, char **__RESTRICT out, size_t *__RESTRICT outleft)
{
	struct conv *c = cd;
	size_t count = 0;
	int ignored = 0;
	if (!in || !*in) {
		/* back to the initial shift state */
		c->in_set = 0;
		c->in_le = -1;
		c->out_bom = 0;
		if (c->to == ISO2022JP && c->out_set) {
			if (!out || !*out) { c->out_set = 0; return 0; }
			if (*outleft < 3) { errno = E2BIG; return (size_t)-1; }
			memcpy(*out, "\x1b(B", 3);
			*out += 3; *outleft -= 3;
			c->out_set = 0;
		}
		return 0;
	}
	while (*inleft) {
		uint32_t wc;
		long l = __iconv_decode(c, (const unsigned char *)*in, *inleft, &wc), k;
		if (l == -2 || l == -3) {
			/* a byte order mark or an escape sequence: no character */
			size_t sk = l == -3 ? 3 : (c->from == UTF32 ? 4 : 2);
			*in += sk; *inleft -= sk;
			continue;
		}
		if (!l) { errno = EINVAL; return (size_t)-1; }
		if (l < 0) {
			if (c->ignore) { (*in)++; (*inleft)--; ignored = 1; continue; }
			errno = EILSEQ;
			return (size_t)-1;
		}
		k = __iconv_encode(c, wc, (unsigned char *)*out, *outleft);
		if (k == -1) {
			if (c->ignore && !c->translit) { *in += l; *inleft -= (size_t)l; ignored = 1; continue; }
			if (!c->translit) { errno = EILSEQ; return (size_t)-1; }
			k = translit(c, wc, (unsigned char *)*out, *outleft);
			if (k == -1) { errno = EILSEQ; return (size_t)-1; }
			if (k >= 0) count++;
		}
		if (k == -2) { errno = E2BIG; return (size_t)-1; }
		*in += l; *inleft -= (size_t)l;
		*out += k; *outleft -= (size_t)k;
	}
	if (ignored) { errno = EILSEQ; return (size_t)-1; }
	return count;
}
