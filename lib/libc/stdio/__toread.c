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
