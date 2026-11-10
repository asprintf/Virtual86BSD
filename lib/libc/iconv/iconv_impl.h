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
