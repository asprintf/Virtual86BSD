/* open_wmemstream(): the bytes written are converted to wide characters
 * in a growing buffer; *bufp and *sizep are updated on every flush and
 * on fclose(). Seeking is by wide characters. */
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "stdio_impl.h"
#include "locale_impl.h"
#include "mb_impl.h"

struct wms {
	wchar_t **bufp;
	size_t *sizep;
	wchar_t *buf;
	size_t pos, len, cap;
	mbstate_t st;
};

static off_t wms_seek(FILE *f, off_t off, int whence)
{
	struct wms *c = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)c->pos : (off_t)c->len;
	if (whence < 0 || whence > 2 || off < -base || off > (off_t)0x7fffffffffffffffLL / 8 - base) { errno = EINVAL; return -1; }
	c->pos = (size_t)(base + off);
	memset(&c->st, 0, sizeof c->st);
	return (off_t)c->pos;
}

static int put1(struct wms *c, wchar_t w)
{
	if (c->pos + 1 >= c->cap) {
		size_t cap = (c->pos + 2) | (c->cap * 2) | 64;
		wchar_t *b = realloc(c->buf, cap * sizeof *b);
		if (!b) return -1;
		wmemset(b + c->cap, 0, cap - c->cap);
		c->buf = b;
		c->cap = cap;
		*c->bufp = b;
	}
	c->buf[c->pos++] = w;
	if (c->pos > c->len) c->len = c->pos;
	c->buf[c->len] = 0;
	*c->sizep = c->pos;
	return 0;
}

static size_t wms_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct wms *c = f->cookie;
	const struct __lcat *ct = CURRENT_CTYPE;
	size_t pend = (size_t)(f->wpos - f->wbase), i;
	if (pend) {
		f->wpos = f->wbase;
		if (wms_write(f, f->wbase, pend) < pend) return 0;
	}
	for (i = 0; i < len; i++) {
		wchar_t w;
		char b = (char)buf[i];
		size_t r = __mbrtowc_l(&w, &b, 1, &c->st, ct);
		if (r == (size_t)-2) continue;
		if (r == (size_t)-1) { memset(&c->st, 0, sizeof c->st); w = 0xfffd; }
		if (put1(c, w)) return i;
	}
	return len;
}

static int wms_close(FILE *f) { (void)f; return 0; }

FILE *open_wmemstream(wchar_t **bufp, size_t *sizep)
{
	FILE *f;
	struct wms *c;
	wchar_t *buf;
	if (!bufp || !sizep) { errno = EINVAL; return 0; }
	if (!(f = malloc(sizeof *f + sizeof *c + UNGET + BUFSIZ))) return 0;
	if (!(buf = calloc(64, sizeof *buf))) { free(f); return 0; }
	memset(f, 0, sizeof *f);
	c = (struct wms *)(f + 1);
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
	f->write = wms_write;
	f->seek = wms_seek;
	f->close = wms_close;
	f->mode = 1;
	return __ofl_add(f);
}
