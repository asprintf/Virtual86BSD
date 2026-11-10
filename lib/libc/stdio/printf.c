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

/* ISO C printf wrappers. */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "stdio_impl.h"

int printf(const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfprintf(stdout, fmt, ap); va_end(ap);
	return r;
}

int fprintf(FILE *__RESTRICT f, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfprintf(f, fmt, ap); va_end(ap);
	return r;
}

int vprintf(const char *__RESTRICT fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }

/* string output: a FILE whose "write" copies into the caller's buffer */
struct sn { char *s; size_t n; };

static size_t sn_write(FILE *f, const unsigned char *s, size_t l)
{
	struct sn *c = f->cookie;
	size_t k = (size_t)(f->wpos - f->wbase);
	unsigned char *b = f->wbase;
	/* first the buffered part, then the new data */
	if (k > c->n) k = c->n;
	memcpy(c->s, b, k); c->s += k; c->n -= k;
	k = l > c->n ? c->n : l;
	memcpy(c->s, s, k); c->s += k; c->n -= k;
	*c->s = 0;
	f->wpos = f->wbase = f->buf;
	return l;
}

int vsnprintf(char *__RESTRICT s, size_t n, const char *__RESTRICT fmt, va_list ap)
{
	unsigned char buf[1];
	char dummy[1];
	struct sn c;
	FILE f;
	int r;
	if (n > INT_MAX) { n = INT_MAX; }
	memset(&f, 0, sizeof f);
	c.s = n ? s : dummy;
	c.n = n ? n - 1 : 0;
	*c.s = 0;
	f.lbf = EOF;
	f.write = sn_write;
	f.buf = buf;
	f.buf_size = 0;
	f.cookie = &c;
	f.fd = -1;
	f.lockcount = 1;           /* never contended: no locking needed */
	r = __vfprintf(&f, fmt, ap);
	if (f.wpos != f.wbase) sn_write(&f, 0, 0);
	return r;
}

int snprintf(char *__RESTRICT s, size_t n, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vsnprintf(s, n, fmt, ap); va_end(ap);
	return r;
}

int vsprintf(char *__RESTRICT s, const char *__RESTRICT fmt, va_list ap)
{
	return vsnprintf(s, INT_MAX, fmt, ap);
}

int sprintf(char *__RESTRICT s, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vsprintf(s, fmt, ap); va_end(ap);
	return r;
}
