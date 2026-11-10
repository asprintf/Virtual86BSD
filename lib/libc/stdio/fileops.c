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

/* Small ISO C stream operations. */
#include <errno.h>
#include <stdlib.h>
#include "stdio_impl.h"

int feof(FILE *f) { int r; FLOCK(f); r = !!(f->flags & F_EOF); FUNLOCK(f); return r; }
int ferror(FILE *f) { int r; FLOCK(f); r = !!(f->flags & F_ERR); FUNLOCK(f); return r; }
void clearerr(FILE *f) { FLOCK(f); f->flags &= ~(F_EOF | F_ERR); FUNLOCK(f); }

int setvbuf(FILE *__RESTRICT f, char *__RESTRICT buf, int type, size_t size)
{
	if (type != _IONBF && type != _IOLBF && type != _IOFBF) return -1;
	/* POSIX allows this only before other operations; for a stream that
	 * was used, flush and start over with the new buffering */
	FLOCK(f);
	if (f->wpos != f->wbase || f->rpos != f->rend) __fflush_unlocked(f);
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;
	FUNLOCK(f);
	f->lbf = -1;
	if (type == _IONBF) {
		f->buf_size = 0;
	} else if (type == _IOLBF || type == _IOFBF) {
		if (buf && size >= UNGET) {
			f->buf = (unsigned char *)buf + UNGET;
			f->buf_size = size - UNGET;
		}
		if (type == _IOLBF && f->buf_size) f->lbf = '\n';
	} else {
		return -1;
	}
	f->flags |= F_SVB;
	return 0;
}

void setbuf(FILE *__RESTRICT f, char *__RESTRICT buf)
{
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

int fgetc(FILE *f) { int c; FLOCK(f); c = getc_unlocked_(f); FUNLOCK(f); return c; }
int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }
int fputc(int c, FILE *f) { int r; FLOCK(f); r = putc_unlocked_(c, f); FUNLOCK(f); return r; }
int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, stdout); }

int ungetc(int c, FILE *f)
{
	if (c == EOF) return EOF;
	FLOCK(f);
	if (!f->rpos) __toread(f);
	if (!f->rpos || f->rpos <= f->buf - UNGET) { FUNLOCK(f); return EOF; }
	*--f->rpos = (unsigned char)c;
	f->flags &= ~F_EOF;
	FUNLOCK(f);
	return (unsigned char)c;
}

size_t fread(void *__RESTRICT dest, size_t size, size_t nmemb, FILE *__RESTRICT f)
{
	unsigned char *d = dest;
	size_t len = size * nmemb, l = len, k;
	if (!size || !nmemb) return 0;
	if (nmemb > (size_t)-1 / size) { errno = EOVERFLOW; return 0; }
	FLOCK(f);
	f->mode |= f->mode - 1;
	if (f->rpos != f->rend) {
		k = (size_t)(f->rend - f->rpos);
		if (k > l) k = l;
		{ size_t i; for (i = 0; i < k; i++) d[i] = f->rpos[i]; }
		f->rpos += k;
		d += k;
		l -= k;
	}
	for (; l; l -= k, d += k) {
		k = __toread(f) ? 0 : f->read(f, d, l);
		if (!k) {
			FUNLOCK(f);
			return (len - l) / size;
		}
	}
	FUNLOCK(f);
	return nmemb;
}

size_t fwrite(const void *__RESTRICT src, size_t size, size_t nmemb, FILE *__RESTRICT f)
{
	size_t k, l = size * nmemb;
	if (!size || !nmemb) return 0;
	FLOCK(f);
	k = __fwritex(src, l, f);
	FUNLOCK(f);
	return k == l ? nmemb : k / size;
}

int fputs(const char *__RESTRICT s, FILE *__RESTRICT f)
{
	size_t l = 0;
	while (s[l]) l++;
	return fwrite(s, 1, l, f) == l || !l ? 0 : EOF;
}

int puts(const char *s)
{
	int r;
	FLOCK(stdout);
	r = -(fputs(s, stdout) < 0 || putc_unlocked_('\n', stdout) < 0);
	FUNLOCK(stdout);
	return r;
}

char *fgets(char *__RESTRICT s, int n, FILE *__RESTRICT f)
{
	char *p = s;
	int c = 0;
	if (n <= 0) { errno = EINVAL; return 0; }
	FLOCK(f);
	if (n == 1) { *s = 0; FUNLOCK(f); return s; }
	n--;
	while (n) {
		if (f->rpos != f->rend) {
			unsigned char *z = f->rpos;
			size_t k, avail = (size_t)(f->rend - f->rpos);
			while (z < f->rend && *z != '\n') z++;
			k = z < f->rend ? (size_t)(z - f->rpos) + 1 : avail;
			if (k > (size_t)n) k = (size_t)n;
			{ size_t i; for (i = 0; i < k; i++) p[i] = (char)f->rpos[i]; }
			f->rpos += k;
			p += k;
			n -= (int)k;
			if (p[-1] == '\n') break;
			continue;
		}
		if ((c = __uflow(f)) == EOF) {
			if (p == s || !(f->flags & F_EOF)) s = 0;
			break;
		}
		n--;
		if ((*p++ = (char)c) == '\n') break;
	}
	if (s) *p = 0;
	FUNLOCK(f);
	return s;
}
