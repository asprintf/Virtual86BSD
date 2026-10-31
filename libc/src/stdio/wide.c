/* Wide-character input and output. A stream holds bytes: wide characters
 * are converted to and from the locale's multibyte characters. The
 * printf and scanf functions run the narrow engines on the format
 * converted to multibyte; for printf, widths and precisions and the
 * result count characters. */
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "stdio_impl.h"
#include "locale_impl.h"
#include "mb_impl.h"

int __vfwprintf_mb(FILE *, const char *, va_list);
int __vfwprintf_u8(FILE *, const char *, va_list);
size_t __u8enc_any(char *, unsigned long);

int fwide(FILE *f, int mode)
{
	FLOCK(f);
	if (mode && !f->mode) f->mode = mode > 0 ? 1 : -1;
	mode = f->mode;
	FUNLOCK(f);
	return mode;
}

static wint_t getwc_locked(FILE *f)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	mbstate_t st = { { 0, 0 } };
	wchar_t w;
	int c;
	char b;
	size_t r;
	if (!f->mode) f->mode = 1;
	for (;;) {
		if ((c = getc_unlocked_(f)) == EOF) {
			if (!mbsinit(&st)) { f->flags |= F_ERR; errno = EILSEQ; }
			return WEOF;
		}
		b = (char)c;
		r = __mbrtowc_l(&w, &b, 1, &st, ct);
		if (r == (size_t)-2) continue;
		if (r == (size_t)-1) { f->flags |= F_ERR; return WEOF; }
		return (wint_t)w;
	}
}

wint_t fgetwc(FILE *f)
{
	wint_t c;
	FLOCK(f);
	c = getwc_locked(f);
	FUNLOCK(f);
	return c;
}

wint_t getwc(FILE *f) { return fgetwc(f); }
wint_t getwchar(void) { return fgetwc(stdin); }

static wint_t putwc_locked(wchar_t c, FILE *f)
{
	char mb[8];
	size_t l, i;
	if (!f->mode) f->mode = 1;
	if ((l = __wcrtomb_l(mb, c, CURRENT_CTYPE)) == (size_t)-1) { f->flags |= F_ERR; return WEOF; }
	for (i = 0; i < l; i++) if (putc_unlocked_(mb[i], f) == EOF) return WEOF;
	return (wint_t)c;
}

wint_t fputwc(wchar_t c, FILE *f)
{
	wint_t r;
	FLOCK(f);
	r = putwc_locked(c, f);
	FUNLOCK(f);
	return r;
}

wint_t putwc(wchar_t c, FILE *f) { return fputwc(c, f); }
wint_t putwchar(wchar_t c) { return fputwc(c, stdout); }

static wint_t ungetwc_locked(wint_t c, FILE *f)
{
	char mb[8];
	size_t l;
	if (c == WEOF) return WEOF;
	if (!f->mode) f->mode = 1;
	if ((l = __wcrtomb_l(mb, (wchar_t)c, CURRENT_CTYPE)) == (size_t)-1) return WEOF;
	if (!f->rpos) __toread(f);
	if (!f->rpos || f->rpos < f->buf - UNGET + l) return WEOF;
	f->rpos -= l;
	memcpy(f->rpos, mb, l);
	f->flags &= ~F_EOF;
	return c;
}

wint_t ungetwc(wint_t c, FILE *f)
{
	FLOCK(f);
	c = ungetwc_locked(c, f);
	FUNLOCK(f);
	return c;
}

wchar_t *fgetws(wchar_t *__RESTRICT s, int n, FILE *__RESTRICT f)
{
	wchar_t *p = s;
	wint_t c;
	int olderr;
	if (n <= 0) return 0;
	FLOCK(f);
	/* only an error during this call makes the result null */
	olderr = f->flags & F_ERR;
	f->flags &= ~F_ERR;
	for (; n > 1; n--) {
		if ((c = getwc_locked(f)) == WEOF) break;
		*p++ = (wchar_t)c;
		if (c == '\n') break;
	}
	*p = 0;
	if (p == s || (f->flags & F_ERR)) s = 0;
	f->flags |= olderr;
	FUNLOCK(f);
	return s;
}

int fputws(const wchar_t *__RESTRICT s, FILE *__RESTRICT f)
{
	int r = 0;
	FLOCK(f);
	for (; *s; s++) if (putwc_locked(*s, f) == WEOF) { r = -1; break; }
	FUNLOCK(f);
	return r;
}

/* the format as a multibyte string (malloc'd), or 0 with errno set */
static char *mbformat(const wchar_t *fmt)
{
	const wchar_t *p = fmt;
	size_t n = wcsrtombs(0, &p, 0, 0);
	char *s;
	if (n == (size_t)-1) return 0;
	if (!(s = malloc(n + 1))) return 0;
	p = fmt;
	wcsrtombs(s, &p, n + 1, 0);
	return s;
}

int vfwprintf(FILE *__RESTRICT f, const wchar_t *__RESTRICT fmt, va_list ap)
{
	char *m = mbformat(fmt);
	int r;
	if (!m) return -1;
	FLOCK(f);
	r = __vfwprintf_mb(f, m, ap);
	FUNLOCK(f);
	free(m);
	return r;
}

int fwprintf(FILE *__RESTRICT f, const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfwprintf(f, fmt, ap); va_end(ap);
	return r;
}

int vwprintf(const wchar_t *__RESTRICT fmt, va_list ap) { return vfwprintf(stdout, fmt, ap); }

int wprintf(const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfwprintf(stdout, fmt, ap); va_end(ap);
	return r;
}

/* swprintf: the format in UTF-8 and the output, UTF-8 (of any wchar_t
 * value: every wide character passes through, as glibc), converted
 * into the wide buffer */
static char *u8format(const wchar_t *fmt)
{
	size_t n = 0, i;
	char b[8], *s;
	for (i = 0; fmt[i]; i++) {
		size_t k = __u8enc_any(b, (unsigned long)fmt[i]);
		if (k == (size_t)-1) { errno = EILSEQ; return 0; }
		n += k;
	}
	if (!(s = malloc(n + 1))) return 0;
	for (i = 0, n = 0; fmt[i]; i++) n += __u8enc_any(s + n, (unsigned long)fmt[i]);
	s[n] = 0;
	return s;
}

struct sw { wchar_t *s; size_t n, k; int over, need; unsigned long acc; };

static size_t sw_conv(struct sw *c, const unsigned char *s, size_t l)
{
	size_t i;
	for (i = 0; i < l; i++) {
		unsigned b = s[i];
		if (c->need) { c->acc = c->acc << 6 | (b & 0x3f); if (--c->need) continue; }
		else if (b < 0x80) c->acc = b;
		else {
			int n = b >= 0xfc ? 5 : b >= 0xf8 ? 4 : b >= 0xf0 ? 3 : b >= 0xe0 ? 2 : 1;
			c->acc = b & (0x3f >> n);
			c->need = n;
			continue;
		}
		if (c->k + 1 < c->n) c->s[c->k++] = (wchar_t)c->acc;
		else c->over = 1;
	}
	return l;
}
static size_t sw_write(FILE *f, const unsigned char *s, size_t l)
{
	struct sw *c = f->cookie;
	sw_conv(c, f->wbase, (size_t)(f->wpos - f->wbase));
	f->wpos = f->wbase = f->buf;
	return sw_conv(c, s, l);
}

int vswprintf(wchar_t *__RESTRICT s, size_t n, const wchar_t *__RESTRICT fmt, va_list ap)
{
	unsigned char buf[256];
	struct sw c;
	FILE f;
	char *m;
	int r;
	if (!n) return -1;
	if (!(m = u8format(fmt))) return -1;
	memset(&f, 0, sizeof f);
	memset(&c, 0, sizeof c);
	c.s = s; c.n = n;
	f.lbf = EOF;
	f.write = sw_write;
	f.buf = buf;
	f.buf_size = sizeof buf;
	f.cookie = &c;
	f.fd = -1;
	f.lockcount = 1;
	r = __vfwprintf_u8(&f, m, ap);
	sw_write(&f, 0, 0);
	free(m);
	s[c.k] = 0;
	if (r < 0) return -1;
	if (c.over) { errno = EOVERFLOW; return -1; }
	return r;
}

int swprintf(wchar_t *__RESTRICT s, size_t n, const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vswprintf(s, n, fmt, ap); va_end(ap);
	return r;
}

/* the sources of the wide scanf engine: a stream (one character
 * pushed back, given back to the stream at the end), a string */
struct fsrc { struct __wsrc s; FILE *f; wint_t back; };

static unsigned fs_get(struct __wsrc *s)
{
	struct fsrc *c = (struct fsrc *)s;
	wint_t r = c->back;
	if (r != WEOF) { c->back = WEOF; return r; }
	return getwc_locked(c->f);
}

static void fs_unget(struct __wsrc *s, unsigned ch) { ((struct fsrc *)s)->back = ch; }

int vfwscanf(FILE *__RESTRICT f, const wchar_t *__RESTRICT fmt, va_list ap)
{
	struct fsrc c;
	int r;
	c.s.get = fs_get;
	c.s.unget = fs_unget;
	c.f = f;
	c.back = WEOF;
	FLOCK(f);
	if (!f->mode) f->mode = 1;
	r = __vfwscanf_core(&c.s, fmt, ap);
	if (c.back != WEOF) ungetwc_locked(c.back, f);
	FUNLOCK(f);
	return r;
}

int fwscanf(FILE *__RESTRICT f, const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfwscanf(f, fmt, ap); va_end(ap);
	return r;
}

int vwscanf(const wchar_t *__RESTRICT fmt, va_list ap) { return vfwscanf(stdin, fmt, ap); }

int wscanf(const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfwscanf(stdin, fmt, ap); va_end(ap);
	return r;
}

struct ssrc { struct __wsrc s; const wchar_t *p; };

static unsigned ss_get(struct __wsrc *s)
{
	struct ssrc *c = (struct ssrc *)s;
	return *c->p ? (unsigned)*c->p++ : WEOF;
}

static void ss_unget(struct __wsrc *s, unsigned ch) { ((struct ssrc *)s)->p--; (void)ch; }

int vswscanf(const wchar_t *__RESTRICT s, const wchar_t *__RESTRICT fmt, va_list ap)
{
	struct ssrc c;
	c.s.get = ss_get;
	c.s.unget = ss_unget;
	c.p = s;
	return __vfwscanf_core(&c.s, fmt, ap);
}

int swscanf(const wchar_t *__RESTRICT s, const wchar_t *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vswscanf(s, fmt, ap); va_end(ap);
	return r;
}
