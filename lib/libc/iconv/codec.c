/* The codecs of iconv and of the multibyte locale codesets: one character
 * at a time between a codeset and Unicode (see iconv.c for the list).
 * The double-byte tables are generated (tools/mkcjk.c); their inverses
 * are made when first needed. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "locale_impl.h"
#include "lock.h"
#include "iconv_impl.h"

#include "jis0208.h"
#include "jis0212.h"
#include "gb2312.h"
#include "gbk.h"
#include "gb18030_2.h"
#include "gb18030.h"
#include "ksc5601.h"
#include "big5.h"

/* ---- the double-byte tables, and their inverses (made when first needed) ---- */

struct inv { uint32_t *v; size_t n; };   /* sorted (code point << 16 | index) */
static volatile int invlock;

static int invcmp(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y;
}

/* the index of the entry for wc in table t of n entries, or -1; for a
 * character with several codes the first, unless pref names another */
static long rev(struct inv *iv, const uint16_t *t, size_t n, const uint32_t (*pref)[2], uint32_t wc)
{
	size_t lo, hi, i, k;
	if (wc > 0xffff || !wc) return -1;
	for (; (*pref)[0]; pref++) if ((*pref)[0] == wc) return (long)(*pref)[1];
	LOCK(&invlock);
	if (!iv->v) {
		uint32_t *v = malloc(n * sizeof *v);
		if (!v) { UNLOCK(&invlock); return -1; }
		for (i = k = 0; i < n; i++) if (t[i]) v[k++] = (uint32_t)t[i] << 16 | (uint32_t)i;
		qsort(v, k, sizeof *v, invcmp);
		iv->n = k;
		iv->v = v;
	}
	UNLOCK(&invlock);
	lo = 0; hi = iv->n;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		uint32_t x = iv->v[m] >> 16;
		if (x < wc) lo = m + 1;
		else if (x > wc) hi = m;
		else {
			/* the first of equal ones: the lowest code */
			while (m > 0 && iv->v[m - 1] >> 16 == wc) m--;
			return (long)(iv->v[m] & 0xffff);
		}
	}
	return -1;
}

static struct inv inv_jis, inv_jis2, inv_gb2312, inv_gbk, inv_gb18030, inv_ksc, inv_big5;

#define N(t) (sizeof t / sizeof *t)

static int big5trail(int b) { return b >= 0x40 && b <= 0x7e ? b - 0x40 : b >= 0xa1 && b <= 0xfe ? b - 0xa1 + 63 : -1; }
static int gbktrail(int b) { return b >= 0x40 && b <= 0xfe && b != 0x7f ? b - 0x40 - (b > 0x7f) : -1; }

/* ---- decoding: the character at s[0..n); its length, 0 if incomplete, -1 if invalid ---- */

static long get16(const unsigned char *s, int le) { return le ? s[0] | s[1] << 8 : s[0] << 8 | s[1]; }
static uint32_t get32(const unsigned char *s, int le)
{
	return le ? (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24
	          : (uint32_t)s[0] << 24 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 8 | (uint32_t)s[3];
}

long __iconv_decode(struct conv *c, const unsigned char *s, size_t n, uint32_t *wc)
{
	uint32_t u;
	long k;
	int le, b;
	switch (c->from) {
	case UTF8: {
		unsigned need, lo = 0x80, hi = 0xbf, i;
		u = s[0];
		if (u < 0x80) { *wc = u; return 1; }
		if (u < 0xc2 || u > 0xf4) return -1;
		if (u < 0xe0) { need = 1; u &= 0x1f; }
		else if (u < 0xf0) { need = 2; if (u == 0xe0) lo = 0xa0; if (u == 0xed) hi = 0x9f; u &= 0xf; }
		else { need = 3; if (u == 0xf0) lo = 0x90; if (u == 0xf4) hi = 0x8f; u &= 7; }
		for (i = 1; i <= need; i++) {
			if (i >= n) return 0;
			if (s[i] < lo || s[i] > hi) return -1;
			u = u << 6 | (s[i] & 0x3f);
			lo = 0x80; hi = 0xbf;
		}
		*wc = u;
		return (long)need + 1;
	}
	case UTF16: case UTF16LE: case UTF16BE: case UCS2: case UCS2LE: case UCS2BE:
		if (n < 2) return 0;
		k = 0;
		if (c->from == UTF16 || c->from == UCS2) {
			if (c->in_le < 0) {
				if (s[0] == 0xff && s[1] == 0xfe) { c->in_le = 1; k = 2; }
				else if (s[0] == 0xfe && s[1] == 0xff) { c->in_le = 0; k = 2; }
				else c->in_le = 1;
				if (k) { *wc = 0xfeff; return -2 - 0; }   /* a mark: consumed, no character */
			}
			le = c->in_le;
		} else le = c->from == UTF16LE || c->from == UCS2LE;
		u = (uint32_t)get16(s, le);
		if (u - 0xdc00 < 0x400) return -1;
		if (u - 0xd800 < 0x400) {
			uint32_t v;
			if (c->from >= UCS2) return -1;
			if (n < 4) return 0;
			v = (uint32_t)get16(s + 2, le);
			if (v - 0xdc00 >= 0x400) return -1;
			*wc = 0x10000 + ((u - 0xd800) << 10) + (v - 0xdc00);
			return 4;
		}
		*wc = u;
		return 2;
	case UTF32: case UTF32LE: case UTF32BE: case UCS4: case UCS4LE: case WCHART:
		if (n < 4) return 0;
		if (c->from == UTF32 && c->in_le < 0) {
			if (s[0] == 0xff && s[1] == 0xfe && !s[2] && !s[3]) { c->in_le = 1; *wc = 0xfeff; return -2; }
			if (!s[0] && !s[1] && s[2] == 0xfe && s[3] == 0xff) { c->in_le = 0; *wc = 0xfeff; return -2; }
			c->in_le = 1;
		}
		le = c->from == UTF32 ? c->in_le : c->from == UTF32LE || c->from == UCS4LE || c->from == WCHART;
		u = get32(s, le);
		if (u > 0x10ffff || u - 0xd800 < 0x800) return -1;
		*wc = u;
		return 4;
	case ASCII:
		if (s[0] >= 0x80) return -1;
		*wc = s[0];
		return 1;
	case SB:
		if (s[0] < 0x80) { *wc = s[0]; return 1; }
		if (!(u = c->fcs->hi[s[0] - 0x80])) return -1;
		*wc = u;
		return 1;
	case EUCJP:
		if (s[0] < 0x80 || (s[0] >= 0x80 && s[0] <= 0x8d) || (s[0] >= 0x90 && s[0] <= 0x9f)) { *wc = s[0]; return 1; }
		if (s[0] != 0x8e && s[0] != 0x8f && (s[0] < 0xa1 || s[0] > 0xfe)) return -1;
		if (n < 2) return 0;
		if (s[0] == 0x8f) {
			if (s[1] < 0xa1 || s[1] > 0xfe) return -1;
			/* rows of JIS X 0212 outside 0xa2-0xed: invalid at once, as glibc */
			if (s[1] < 0xa2 || s[1] > 0xed) return -1;
			if (n < 3) return 0;
			if (s[1] < 0xa1 || s[1] > 0xfe || s[2] < 0xa1 || s[2] > 0xfe) return -1;
			if (!(u = jis0212[(s[1] - 0xa1) * 94 + (s[2] - 0xa1)])) return -1;
			*wc = u;
			return 3;
		}
		if (s[0] == 0x8e) {
			if (s[1] < 0xa1 || s[1] > 0xdf) return -1;
			*wc = 0xff61 + (s[1] - 0xa1);
			return 2;
		}
		if (s[0] < 0xa1 || s[0] > 0xfe || s[1] < 0xa1 || s[1] > 0xfe) return -1;
		if (!(u = jis0208[(s[0] - 0xa1) * 94 + (s[1] - 0xa1)])) return -1;
		*wc = u;
		return 2;
	case SJIS: {
		int r, col;
		/* the single bytes are JIS X 0201: yen sign and overline */
		if (s[0] == 0x5c) { *wc = 0xa5; return 1; }
		if (s[0] == 0x7e) { *wc = 0x203e; return 1; }
		if (s[0] < 0x80) { *wc = s[0]; return 1; }
		if (s[0] >= 0xa1 && s[0] <= 0xdf) { *wc = 0xff61 + (s[0] - 0xa1); return 1; }
		if (!((s[0] >= 0x81 && s[0] <= 0x9f) || (s[0] >= 0xe0 && s[0] <= 0xea))) return -1;
		if (n < 2) return 0;
		if (s[1] < 0x40 || s[1] == 0x7f || s[1] > 0xfc) return -1;
		r = (s[0] < 0xa0 ? s[0] - 0x81 : s[0] - 0xc1) * 2;
		col = s[1] - 0x40 - (s[1] > 0x7f);
		if (col >= 94) { r++; col -= 94; }
		if (!(u = jis0208[r * 94 + col])) return -1;
		*wc = u;
		return 2;
	}
	case ISO2022JP:
		if (s[0] == 0x1b) {
			if (n < 3) return 0;
			if (s[1] == '(' && s[2] == 'B') c->in_set = 0;
			else if (s[1] == '(' && s[2] == 'J') c->in_set = 1;
			else if (s[1] == '$' && (s[2] == '@' || s[2] == 'B')) c->in_set = 2;
			else return -1;
			return -3;
		}
		if (s[0] >= 0x80) return -1;
		if (c->in_set == 2) {
			if (s[0] == '\n' || s[0] == '\r') { *wc = s[0]; return 1; }
			if (n < 2) return 0;
			if (s[0] < 0x21 || s[0] > 0x7e || s[1] < 0x21 || s[1] > 0x7e) return -1;
			if (!(u = jis0208[(s[0] - 0x21) * 94 + (s[1] - 0x21)])) return -1;
			*wc = u;
			return 2;
		}
		u = s[0];
		if (c->in_set == 1) { if (u == 0x5c) u = 0xa5; else if (u == 0x7e) u = 0x203e; }
		*wc = u;
		return 1;
	case EUCCN: case EUCKR:
		if (s[0] < 0x80 || (c->from == EUCKR && s[0] <= 0x9f)) { *wc = s[0]; return 1; }
		if (s[0] < 0xa1 || s[0] > 0xfe) return -1;
		if (n < 2) return 0;
		if (s[0] < 0xa1 || s[0] > 0xfe || s[1] < 0xa1 || s[1] > 0xfe) return -1;
		u = (c->from == EUCCN ? gb2312 : ksc5601)[(s[0] - 0xa1) * 94 + (s[1] - 0xa1)];
		if (!u) return -1;
		*wc = u;
		return 2;
	case GBK: case GB18030:
		if (s[0] < 0x80) { *wc = s[0]; return 1; }
		if (s[0] == 0xff || (s[0] == 0x80 && c->from == GB18030)) return -1;
		if (s[0] == 0x80 && c->from == GBK) { *wc = 0x20ac; return 1; }   /* CP936 */
		if (n < 2) return 0;
		if (c->from == GB18030 && s[1] >= 0x30 && s[1] <= 0x39) {
			long idx;
			if (n < 4) return 0;
			if (s[2] < 0x81 || s[2] > 0xfe || s[3] < 0x30 || s[3] > 0x39) return -1;
			idx = (((long)(s[0] - 0x81) * 10 + (s[1] - 0x30)) * 126 + (s[2] - 0x81)) * 10 + (s[3] - 0x30);
			if (s[0] >= 0x90) {
				idx -= (((long)(0x90 - 0x81) * 10) * 126) * 10;
				if (idx > 0xfffff) return -1;
				*wc = 0x10000 + (uint32_t)idx;
				return 4;
			}
			{
				size_t lo = 0, hi = N(gb18030_runs);
				while (hi - lo > 1) {
					size_t m = (lo + hi) / 2;
					if ((long)gb18030_runs[m][0] <= idx) lo = m; else hi = m;
				}
				if (idx >= 39420 || !gb18030_runs[lo][1]) return -1;
				*wc = gb18030_runs[lo][1] + (uint32_t)(idx - (long)gb18030_runs[lo][0]);
				return 4;
			}
		}
		if ((b = gbktrail(s[1])) < 0) return -1;
		u = (c->from == GBK ? gbk : gb18030_2)[(s[0] - 0x81) * 190 + b];
		if (!u) return -1;
		*wc = u;
		return 2;
	case BIG5:
		if (s[0] <= 0x80) { *wc = s[0]; return 1; }
		if (s[0] < 0xa1 || s[0] > 0xf9) return -1;
		if (n < 2) return 0;
		if ((b = big5trail(s[1])) < 0) return -1;
		if (!(u = big5[(s[0] - 0xa1) * 157 + b])) return -1;
		*wc = u;
		return 2;
	}
	return -1;
}

/* ---- encoding: into o[0..n); the length, -1 if the codeset lacks it, -2 if no room ---- */

static void put16(unsigned char *o, uint32_t v, int le)
{
	if (le) { o[0] = (unsigned char)v; o[1] = (unsigned char)(v >> 8); }
	else { o[0] = (unsigned char)(v >> 8); o[1] = (unsigned char)v; }
}

static void put32(unsigned char *o, uint32_t v, int le)
{
	int i;
	for (i = 0; i < 4; i++) o[le ? i : 3 - i] = (unsigned char)(v >> (8 * i));
}

long __iconv_encode(struct conv *c, uint32_t wc, unsigned char *o, size_t n)
{
	long k;
	int le;
	/* the tag characters (U+E0000-E007F) are dropped by the codesets that
	 * lack them, as glibc does */
	if (wc >> 7 == 0xe0000 >> 7 && c->to >= ASCII && c->to != GB18030) return 0;
	switch (c->to) {
	case UTF8:
		if (wc < 0x80) { if (n < 1) return -2; o[0] = (unsigned char)wc; return 1; }
		if (wc < 0x800) { if (n < 2) return -2; o[0] = (unsigned char)(0xc0 | wc >> 6); o[1] = (unsigned char)(0x80 | (wc & 63)); return 2; }
		if (wc < 0x10000) {
			if (n < 3) return -2;
			o[0] = (unsigned char)(0xe0 | wc >> 12); o[1] = (unsigned char)(0x80 | (wc >> 6 & 63)); o[2] = (unsigned char)(0x80 | (wc & 63));
			return 3;
		}
		if (n < 4) return -2;
		o[0] = (unsigned char)(0xf0 | wc >> 18); o[1] = (unsigned char)(0x80 | (wc >> 12 & 63));
		o[2] = (unsigned char)(0x80 | (wc >> 6 & 63)); o[3] = (unsigned char)(0x80 | (wc & 63));
		return 4;
	case UTF16: case UTF16LE: case UTF16BE: case UCS2: case UCS2LE: case UCS2BE:
		le = c->to == UTF16 || c->to == UTF16LE || c->to == UCS2 || c->to == UCS2LE;
		k = 0;
		if (c->to == UTF16 && !c->out_bom) k = 2;
		if (wc >= 0x10000) {
			if (c->to >= UCS2) return -1;
			if (n < (size_t)k + 4) return -2;
			put16(o + k, 0xd800 + ((wc - 0x10000) >> 10), le);
			put16(o + k + 2, 0xdc00 + (wc & 0x3ff), le);
			k += 4;
		} else {
			if (n < (size_t)k + 2) return -2;
			put16(o + k, wc, le);
			k += 2;
		}
		if (c->to == UTF16 && !c->out_bom) { put16(o, 0xfeff, 1); c->out_bom = 1; }
		return k;
	case UTF32: case UTF32LE: case UTF32BE: case UCS4: case UCS4LE: case WCHART:
		le = c->to != UTF32BE && c->to != UCS4;
		k = c->to == UTF32 && !c->out_bom ? 4 : 0;
		if (n < (size_t)k + 4) return -2;
		if (k) { put32(o, 0xfeff, 1); c->out_bom = 1; }
		put32(o + k, wc, le);
		return k + 4;
	case ASCII:
		if (wc >= 0x80) return -1;
		if (n < 1) return -2;
		o[0] = (unsigned char)wc;
		return 1;
	case SB: {
		int b = __charset_from_wc(c->tcs, wc);
		if (b < 0) return -1;
		if (n < 1) return -2;
		o[0] = (unsigned char)b;
		return 1;
	}
	case EUCJP: case SJIS: {
		long i;
		if (wc == 0xa5) wc = 0x5c;            /* as glibc */
		else if (wc == 0x203e) wc = 0x7e;
		if (wc < 0x80 || (c->to == EUCJP && ((wc >= 0x80 && wc <= 0x8d) || (wc >= 0x90 && wc <= 0x9f)))) {
			if (n < 1) return -2;
			o[0] = (unsigned char)wc;
			return 1;
		}
		if (c->to == SJIS && wc >= 0xffe0 && wc <= 0xffe2) {
			if (n < 2) return -2;
			o[0] = 0x81; o[1] = (unsigned char)(wc == 0xffe0 ? 0x91 : wc == 0xffe1 ? 0x92 : 0xca);
			return 2;
		}
		if (wc >= 0xff61 && wc <= 0xff9f) {
			if (c->to == SJIS) { if (n < 1) return -2; o[0] = (unsigned char)(wc - 0xff61 + 0xa1); return 1; }
			if (n < 2) return -2;
			o[0] = 0x8e; o[1] = (unsigned char)(wc - 0xff61 + 0xa1);
			return 2;
		}
		if ((i = rev(&inv_jis, jis0208, N(jis0208), jis0208_pref, wc)) < 0) {
			if (c->to == SJIS || (i = rev(&inv_jis2, jis0212, N(jis0212), jis0212_pref, wc)) < 0) return -1;
			if (n < 3) return -2;
			o[0] = 0x8f; o[1] = (unsigned char)(0xa1 + i / 94); o[2] = (unsigned char)(0xa1 + i % 94);
			return 3;
		}
		if (n < 2) return -2;
		if (c->to == EUCJP) { o[0] = (unsigned char)(0xa1 + i / 94); o[1] = (unsigned char)(0xa1 + i % 94); return 2; }
		{
			int r = (int)(i / 94), col = (int)(i % 94);
			o[0] = (unsigned char)(r / 2 + (r < 62 ? 0x81 : 0xc1));
			col += r % 2 ? 94 : 0;
			o[1] = (unsigned char)(col + 0x40 + (col >= 0x3f));
			return 2;
		}
	}
	case ISO2022JP: {
		int set;
		long i = -1;
		unsigned char seq[3];
		if (wc < 0x80 && wc != 0x5c && wc != 0x7e) set = c->out_set == 1 ? 1 : 0;
		else if (wc < 0x80) set = 0;
		else if (wc == 0xa5 || wc == 0x203e) set = 1;
		else if ((i = rev(&inv_jis, jis0208, N(jis0208), jis0208_pref, wc)) >= 0) set = 2;
		else return -1;
		if (wc == '\n' || wc == '\r') set = 0;
		k = set != c->out_set ? 3 : 0;
		if (n < (size_t)k + (set == 2 ? 2 : 1)) return -2;
		if (k) {
			seq[0] = 0x1b;
			seq[1] = set == 2 ? '$' : '(';
			seq[2] = set == 2 ? 'B' : set == 1 ? 'J' : 'B';
			memcpy(o, seq, 3);
			c->out_set = set;
		}
		if (set == 2) { o[k] = (unsigned char)(0x21 + i / 94); o[k + 1] = (unsigned char)(0x21 + i % 94); return k + 2; }
		o[k] = (unsigned char)(set == 1 ? (wc == 0xa5 ? 0x5c : wc == 0x203e ? 0x7e : wc) : wc);
		return k + 1;
	}
	case EUCCN: case EUCKR: {
		long i;
		if (wc < 0x80 || (c->to == EUCKR && wc <= 0x9f)) { if (n < 1) return -2; o[0] = (unsigned char)wc; return 1; }
		if (c->to == EUCKR && wc == 0x20a9) wc = 0xffe6;   /* the won sign, as glibc */
		i = c->to == EUCCN ? rev(&inv_gb2312, gb2312, N(gb2312), gb2312_pref, wc) : rev(&inv_ksc, ksc5601, N(ksc5601), ksc5601_pref, wc);
		if (i < 0) return -1;
		if (n < 2) return -2;
		o[0] = (unsigned char)(0xa1 + i / 94); o[1] = (unsigned char)(0xa1 + i % 94);
		return 2;
	}
	case GBK: case GB18030: {
		long i;
		if (wc < 0x80) { if (n < 1) return -2; o[0] = (unsigned char)wc; return 1; }
		if (wc == 0x20ac && c->to == GBK) { if (n < 1) return -2; o[0] = 0x80; return 1; }
		i = c->to == GBK ? rev(&inv_gbk, gbk, N(gbk), gbk_pref, wc) : rev(&inv_gb18030, gb18030_2, N(gb18030_2), gb18030_2_pref, wc);
		if (i >= 0) {
			int t = (int)(i % 190);
			if (n < 2) return -2;
			o[0] = (unsigned char)(0x81 + i / 190);
			o[1] = (unsigned char)(t + 0x40 + (t >= 0x3f));
			return 2;
		}
		if (c->to == GBK) return -1;
		{
			long idx;
			size_t j;
			if (wc >= 0x10000) idx = (long)(wc - 0x10000) + (((long)(0x90 - 0x81) * 10) * 126) * 10;
			else {
				for (j = 0, idx = -1; j + 1 < N(gb18030_runs); j++)
					if (gb18030_runs[j][1] && wc >= gb18030_runs[j][1] &&
					    wc - gb18030_runs[j][1] < gb18030_runs[j + 1][0] - gb18030_runs[j][0]) {
						idx = (long)gb18030_runs[j][0] + (long)(wc - gb18030_runs[j][1]);
						break;
					}
				if (idx < 0) return -1;
			}
			if (n < 4) return -2;
			o[3] = (unsigned char)(0x30 + idx % 10); idx /= 10;
			o[2] = (unsigned char)(0x81 + idx % 126); idx /= 126;
			o[1] = (unsigned char)(0x30 + idx % 10); idx /= 10;
			o[0] = (unsigned char)(0x81 + idx);
			return 4;
		}
	}
	case BIG5: {
		long i;
		int t;
		if (wc <= 0x80) { if (n < 1) return -2; o[0] = (unsigned char)wc; return 1; }
		if ((i = rev(&inv_big5, big5, N(big5), big5_pref, wc)) < 0) return -1;
		if (n < 2) return -2;
		t = (int)(i % 157);
		o[0] = (unsigned char)(0xa1 + i / 157);
		o[1] = (unsigned char)(t < 63 ? 0x40 + t : 0xa1 + t - 63);
		return 2;
	}
	}
	return -1;
}


/* ---- the multibyte codesets of locales ---- */

int __mbcs_kind(const char *cs)
{
	static const struct { const char *name; int kind; } k[] = {
		{ "EUC-JP", EUCJP }, { "EUCJP", EUCJP }, { "SHIFT_JIS", SJIS }, { "SHIFT-JIS", SJIS },
		{ "SJIS", SJIS }, { "EUC-CN", EUCCN }, { "GB2312", EUCCN }, { "GBK", GBK },
		{ "GB18030", GB18030 }, { "EUC-KR", EUCKR }, { "BIG5", BIG5 }, { "BIG-5", BIG5 },
	};
	unsigned i;
	const char *a, *b;
	for (i = 0; i < sizeof k / sizeof *k; i++) {
		/* the same without case */
		for (a = k[i].name, b = cs; *a && (*a == *b || (*b >= 'a' && *b <= 'z' && *b - 32 == *a)); a++, b++)
			;
		if (!*a && !*b) return k[i].kind;
	}
	return -1;
}

long __mbcs_decode(int kind, const unsigned char *s, size_t n, uint32_t *wc)
{
	struct conv c;
	memset(&c, 0, sizeof c);
	c.from = kind;
	return __iconv_decode(&c, s, n, wc);
}

long __mbcs_encode(int kind, uint32_t wc, unsigned char *o)
{
	struct conv c;
	memset(&c, 0, sizeof c);
	c.to = kind;
	return __iconv_encode(&c, wc, o, 4);
}

/* A lead byte that glibc, given it alone, takes for the start of an
 * incomplete character, though no character starts with it: mbrtowc says
 * "incomplete" for it too (and "invalid" with the next byte). */
int __mbcs_waits(int kind, unsigned b)
{
	switch (kind) {
	case EUCJP: return b == 0xa0;
	case EUCCN: return b == 0x8e || b == 0x8f;
	case EUCKR: return b == 0xff;
	}
	return 0;
}
