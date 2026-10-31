/* fmemopen(): a stream on a caller-supplied (or private) buffer, with the
 * size, position and end-of-data semantics of POSIX.1-2024. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "stdio_impl.h"

struct mem {
	unsigned char *buf;
	size_t size, pos, len;
	int mode;              /* 'r', 'w', 'a' */
	int owned;
};

static off_t mseek(FILE *f, off_t off, int whence)
{
	struct mem *m = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)m->pos : (off_t)m->len;
	if (whence < 0 || whence > 2 || off < -base || off > (off_t)m->size - base) { errno = EINVAL; return -1; }
	return (off_t)(m->pos = (size_t)(base + off));
}

static size_t mread(FILE *f, unsigned char *buf, size_t len)
{
	struct mem *m = f->cookie;
	size_t rem = m->pos < m->len ? m->len - m->pos : 0;
	if (len > rem) { len = rem; f->flags |= F_EOF; }
	memcpy(buf, m->buf + m->pos, len);
	m->pos += len;
	/* also fill the stream buffer */
	rem = m->len - m->pos;
	if (rem > f->buf_size) rem = f->buf_size;
	memcpy(f->buf, m->buf + m->pos, rem);
	f->rpos = f->buf;
	f->rend = f->buf + rem;
	m->pos += rem;
	return len;
}

static size_t mwrite(FILE *f, const unsigned char *buf, size_t len)
{
	struct mem *m = f->cookie;
	size_t rem, pend = (size_t)(f->wpos - f->wbase);
	if (pend) {
		f->wpos = f->wbase;
		if (mwrite(f, f->wbase, pend) < pend) return 0;
	}
	if (m->mode == 'a') m->pos = m->len;
	rem = m->size - m->pos;
	if (len > rem) len = rem;
	memcpy(m->buf + m->pos, buf, len);
	m->pos += len;
	if (m->pos > m->len) {
		m->len = m->pos;
		/* keep the data null-terminated if there is room */
		if (m->len < m->size) m->buf[m->len] = 0;
		else if (m->mode != 'r' && m->size) m->buf[m->size - 1] = 0, m->len = m->size;
	}
	return len;
}

static int mclose(FILE *f)
{
	struct mem *m = f->cookie;
	if (m->owned) free(m->buf);
	return 0;
}

FILE *fmemopen(void *__RESTRICT buf, size_t size, const char *__RESTRICT mode)
{
	FILE *f;
	struct mem *m;
	int plus = !!strchr(mode, '+');
	if (!strchr("rwa", *mode) || !size) { errno = EINVAL; return 0; }
	if (!buf && !plus) { errno = EINVAL; return 0; }
	f = malloc(sizeof *f + sizeof *m + UNGET + BUFSIZ);
	if (!f) return 0;
	memset(f, 0, sizeof *f);
	m = (struct mem *)(f + 1);
	memset(m, 0, sizeof *m);
	f->cookie = m;
	f->fd = -1;
	f->lbf = -1;
	f->buf = (unsigned char *)(m + 1) + UNGET;
	f->buf_size = BUFSIZ;
	m->size = size;
	m->mode = *mode;
	if (!buf) {
		if (!(m->buf = calloc(1, size))) { free(f); return 0; }
		m->owned = 1;
	} else {
		m->buf = buf;
	}
	if (!plus) f->flags = *mode == 'r' ? F_NOWR : F_NORD;
	if (*mode == 'r') m->len = size;
	else if (*mode == 'a') { m->len = buf ? strnlen((char *)m->buf, size) : 0; m->pos = m->len; }
	else if (buf) *m->buf = 0;
	f->read = mread;
	f->write = mwrite;
	f->seek = mseek;
	f->close = mclose;
	return __ofl_add(f);
}
