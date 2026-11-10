#include <errno.h>
#include <limits.h>
#include <string.h>
#include "stdio_impl.h"

off_t __ftello_unlocked(FILE *f)
{
	off_t pos = f->seek(f, 0, (f->flags & F_APP) && f->wpos != f->wbase ? SEEK_END : SEEK_CUR);
	if (pos < 0) return pos;
	if (f->rend) pos += f->rpos - f->rend;          /* unread input */
	else if (f->wbase) pos += f->wpos - f->wbase;   /* unwritten output */
	return pos;
}

int __fseeko_unlocked(FILE *f, off_t off, int whence)
{
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) { errno = EINVAL; return -1; }
	if (whence == SEEK_CUR && f->rend) off -= f->rend - f->rpos;
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return -1;
	}
	f->wpos = f->wbase = f->wend = 0;
	if (f->seek(f, off, whence) < 0) return -1;
	f->rpos = f->rend = 0;
	f->flags &= ~F_EOF;
	return 0;
}

long ftell(FILE *f)
{
	off_t pos;
	FLOCK(f);
	pos = __ftello_unlocked(f);
	FUNLOCK(f);
	if (pos > LONG_MAX) { errno = EOVERFLOW; return -1; }
	return (long)pos;
}

int fseek(FILE *f, long off, int whence)
{
	int r;
	FLOCK(f);
	r = __fseeko_unlocked(f, off, whence);
	FUNLOCK(f);
	return r;
}

void rewind(FILE *f)
{
	FLOCK(f);
	__fseeko_unlocked(f, 0, SEEK_SET);
	f->flags &= ~F_ERR;
	FUNLOCK(f);
}

int fgetpos(FILE *__RESTRICT f, fpos_t *__RESTRICT pos)
{
	off_t off;
	FLOCK(f);
	off = __ftello_unlocked(f);
	FUNLOCK(f);
	if (off < 0) return -1;
	memcpy(pos, &off, sizeof off);
	return 0;
}

int fsetpos(FILE *f, const fpos_t *pos)
{
	off_t off;
	int r;
	memcpy(&off, pos, sizeof off);
	FLOCK(f);
	r = __fseeko_unlocked(f, off, SEEK_SET);
	FUNLOCK(f);
	return r;
}
