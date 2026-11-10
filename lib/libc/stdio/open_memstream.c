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

/* open_memstream(): a growing buffer; *bufp and *sizep are updated on
 * every flush and on fclose(). */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "stdio_impl.h"

struct ms {
	char **bufp;
	size_t *sizep;
	char *buf;
	size_t pos, len, cap;
};

static off_t ms_seek(FILE *f, off_t off, int whence)
{
	struct ms *c = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)c->pos : (off_t)c->len;
	if (whence < 0 || whence > 2 || off < -base || off > (off_t)0x7fffffffffffffffLL - base) { errno = EINVAL; return -1; }
	c->pos = (size_t)(base + off);
	return (off_t)c->pos;
}

static size_t ms_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct ms *c = f->cookie;
	size_t pend = (size_t)(f->wpos - f->wbase);
	if (pend) {
		f->wpos = f->wbase;
		if (ms_write(f, f->wbase, pend) < pend) return 0;
	}
	if (len + c->pos >= c->cap) {
		size_t cap = (len + c->pos + 1) | (c->cap * 2) | 64;
		char *b = realloc(c->buf, cap);
		if (!b) return 0;
		memset(b + c->cap, 0, cap - c->cap);
		c->buf = b;
		c->cap = cap;
		*c->bufp = b;
	}
	memcpy(c->buf + c->pos, buf, len);
	c->pos += len;
	if (c->pos >= c->len) c->len = c->pos;
	c->buf[c->len] = 0;
	*c->sizep = c->pos < c->len ? c->pos : c->len;
	return len;
}

static int ms_close(FILE *f) { (void)f; return 0; }

FILE *open_memstream(char **bufp, size_t *sizep)
{
	FILE *f;
	struct ms *c;
	char *buf;
	if (!bufp || !sizep) { errno = EINVAL; return 0; }
	if (!(f = malloc(sizeof *f + sizeof *c + UNGET + BUFSIZ))) return 0;
	if (!(buf = calloc(1, 64))) { free(f); return 0; }
	memset(f, 0, sizeof *f);
	c = (struct ms *)(f + 1);
	memset(c, 0, sizeof *c);
	c->bufp = bufp;
	c->sizep = sizep;
	c->buf = *bufp = buf;
	c->cap = 64;
	*sizep = 0;
	f->cookie = c;
	f->flags = F_NORD;
	f->fd = -1;
	f->lbf = -1;
	f->buf = (unsigned char *)(c + 1) + UNGET;
	f->buf_size = BUFSIZ;
	f->write = ms_write;
	f->seek = ms_seek;
	f->close = ms_close;
	f->mode = -1;
	return __ofl_add(f);
}
