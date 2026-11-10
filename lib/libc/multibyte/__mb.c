/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Conversions between multibyte and wide characters in a locale's
 * codeset: UTF-8; EUC-JP, SHIFT_JIS, GB18030 ... (the codecs of iconv);
 * the POSIX locale (one byte each, bytes 0x80-0xff as the
 * wide characters 0xdf80-0xdfff, so that every byte string converts and
 * back); single-byte codesets. */
#include <wchar.h>
#include <errno.h>
#include <string.h>
#include "locale_impl.h"
#include "mb_impl.h"
#include "../iconv/iconv_impl.h"

/* UTF-8: the state holds the bytes still needed (bits 0-3), the range of
 * the next byte (bits 8-15, 16-23) and the value so far. */
size_t __mbrtowc_l(wchar_t *pwc, const char *src, size_t n, mbstate_t *st, const struct __lcat *ct)
{
	const unsigned char *s = (const unsigned char *)src;
	unsigned need, lo, hi, v, c;
	size_t i;
	if (!s) { s = (const unsigned char *)""; n = 1; pwc = 0; }
	if (!n) return (size_t)-2;
	if (ct->cs == CS_MB) {
		/* the codecs of iconv; the state keeps the bytes of an incomplete
		 * character: their number (bits 0-3, with bit 4 set) and the bytes */
		unsigned char buf[4];
		size_t have = st->__st[0] & 15, take, k;
		uint32_t u;
		long r;
		if (have > 3) goto ilseq;
		for (k = 0; k < have; k++) buf[k] = (unsigned char)(st->__st[1] >> (8 * k));
		for (take = 0; take < n && have + take < 4;) {
			buf[have + take] = s[take];
			if (!s[take++]) break;
		}
		r = __mbcs_decode(ct->mbkind, buf, have + take, &u);
		if (!r && take == n && have + take < 4) {
			for (k = 0; k < take; k++) st->__st[1] |= (unsigned)buf[have + k] << (8 * (have + k));
			st->__st[0] = 16 | (unsigned)(have + take);
			return (size_t)-2;
		}
		if (r < 0 && !have && take == 1 && n == 1 && __mbcs_waits(ct->mbkind, buf[0])) {
			st->__st[1] = buf[0];
			st->__st[0] = 16 | 1;
			return (size_t)-2;
		}
		if (r <= (long)have) goto ilseq;
		st->__st[0] = st->__st[1] = 0;
		if (pwc) *pwc = (wchar_t)u;
		return u ? (size_t)r - have : 0;
	}
	if (ct->cs != CS_UTF8) {
		wchar_t w;
		if (st->__st[0]) { errno = EILSEQ; return (size_t)-1; }
		c = *s;
		if (c < 0x80) w = (wchar_t)c;
		else if (ct->cs == CS_C) w = (wchar_t)(0xdf00 + c);
		else if (!(w = ct->charset->hi[c - 0x80])) { errno = EILSEQ; return (size_t)-1; }
		if (pwc) *pwc = w;
		return c != 0;
	}
	need = st->__st[0] & 15;
	i = 0;
	if (!need) {
		c = s[0];
		if (c < 0x80) { if (pwc) *pwc = (wchar_t)c; return c != 0; }
		if (c < 0xc2 || c > 0xf4) goto ilseq;
		if (c < 0xe0) { need = 1; v = c & 0x1f; lo = 0x80; hi = 0xbf; }
		else if (c < 0xf0) {
			need = 2; v = c & 0x0f;
			lo = c == 0xe0 ? 0xa0 : 0x80;
			hi = c == 0xed ? 0x9f : 0xbf;
		} else {
			need = 3; v = c & 0x07;
			lo = c == 0xf0 ? 0x90 : 0x80;
			hi = c == 0xf4 ? 0x8f : 0xbf;
		}
		i = 1;
	} else {
		lo = (st->__st[0] >> 8) & 0xff;
		hi = (st->__st[0] >> 16) & 0xff;
		v = st->__st[1];
	}
	for (; need; i++) {
		if (i == n) {
			st->__st[0] = need | lo << 8 | hi << 16;
			st->__st[1] = v;
			return (size_t)-2;
		}
		c = s[i];
		if (c < lo || c > hi) goto ilseq;
		v = v << 6 | (c & 0x3f);
		need--;
		lo = 0x80; hi = 0xbf;
	}
	st->__st[0] = st->__st[1] = 0;
	if (pwc) *pwc = (wchar_t)v;
	return i;
ilseq:
	st->__st[0] = st->__st[1] = 0;
	errno = EILSEQ;
	return (size_t)-1;
}

size_t __wcrtomb_l(char *s, wchar_t wc, const struct __lcat *ct)
{
	unsigned w = (unsigned)wc;
	int b;
	/* the tag characters: nothing, outside UTF-8 (glibc) */
	if (w >> 7 == 0xe0000 >> 7 && ct->cs == CS_8BIT) return 0;   /* (CS_MB: the codec) */
	if (ct->cs == CS_MB) {
		unsigned char buf[4];
		long r = __mbcs_encode(ct->mbkind, w, buf);
		if (r < 0) goto ilseq;
		memcpy(s, buf, (size_t)r);
		return (size_t)r;
	}
	if (ct->cs != CS_UTF8) {
		if (w < 0x80) { *s = (char)w; return 1; }
		if (ct->cs == CS_C) {
			if (w - 0xdf80 < 0x80) { *s = (char)(w - 0xdf00); return 1; }
		} else if ((b = __charset_from_wc(ct->charset, w)) >= 0) {
			*s = (char)b;
			return 1;
		}
		errno = EILSEQ;
		return (size_t)-1;
	}
	if (w < 0x80) { *s = (char)w; return 1; }
	if (w < 0x800) {
		s[0] = (char)(0xc0 | w >> 6);
		s[1] = (char)(0x80 | (w & 0x3f));
		return 2;
	}
	if (w < 0x10000) {
		if (w - 0xd800 < 0x800) goto ilseq;
		s[0] = (char)(0xe0 | w >> 12);
		s[1] = (char)(0x80 | (w >> 6 & 0x3f));
		s[2] = (char)(0x80 | (w & 0x3f));
		return 3;
	}
	if (w < 0x110000) {
		s[0] = (char)(0xf0 | w >> 18);
		s[1] = (char)(0x80 | (w >> 12 & 0x3f));
		s[2] = (char)(0x80 | (w >> 6 & 0x3f));
		s[3] = (char)(0x80 | (w & 0x3f));
		return 4;
	}
ilseq:
	errno = EILSEQ;
	return (size_t)-1;
}

/* for printf and friends: the current locale */
size_t __wcrtomb_internal(char *s, wchar_t wc)
{
	return __wcrtomb_l(s, wc, CURRENT_CTYPE);
}
