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

#include "stdio_impl.h"

int __toread(FILE *f)
{
	f->mode |= f->mode - 1;   /* byte-oriented unless already set */
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	f->wpos = f->wbase = f->wend = 0;
	if (f->flags & F_NORD) { f->flags |= F_ERR; return EOF; }
	f->rpos = f->rend = f->buf + f->buf_size;
	return (f->flags & F_EOF) ? EOF : 0;
}

int __towrite(FILE *f)
{
	f->mode |= f->mode - 1;
	if (f->flags & F_NOWR) { f->flags |= F_ERR; return EOF; }
	f->rpos = f->rend = 0;
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;
	return 0;
}

int __uflow(FILE *f)
{
	unsigned char c;
	if (!__toread(f) && f->read(f, &c, 1) == 1) return c;
	return EOF;
}

int __overflow(FILE *f, int ch)
{
	unsigned char c = (unsigned char)ch;
	if (!f->wend && __towrite(f)) return EOF;
	if (f->wpos != f->wend && c != f->lbf) return *f->wpos++ = c;
	if (f->write(f, &c, 1) != 1) return EOF;
	return c;
}

/* write l bytes, honouring line buffering; the stream is locked */
size_t __fwritex(const unsigned char *s, size_t l, FILE *f)
{
	size_t i = 0;
	if (!f->wend && __towrite(f)) return 0;
	if (l > (size_t)(f->wend - f->wpos)) return f->write(f, s, l);
	if (f->lbf >= 0) {
		/* write up to the last newline directly */
		for (i = l; i && s[i - 1] != '\n'; i--);
		if (i) {
			size_t n = f->write(f, s, i);
			if (n < i) return n;
			s += i;
			l -= i;
		}
	}
	{
		unsigned char *d = f->wpos;
		size_t k;
		for (k = 0; k < l; k++) d[k] = s[k];
		f->wpos += l;
	}
	return l + i;
}
