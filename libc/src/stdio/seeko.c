#include <errno.h>
#include "stdio_impl.h"

off_t ftello(FILE *f)
{
	off_t pos;
	FLOCK(f);
	pos = __ftello_unlocked(f);
	FUNLOCK(f);
	return pos;
}

int fseeko(FILE *f, off_t off, int whence)
{
	int r;
	FLOCK(f);
	r = __fseeko_unlocked(f, off, whence);
	FUNLOCK(f);
	return r;
}

int fileno(FILE *f)
{
	int fd;
	FLOCK(f);
	fd = f->fd;
	FUNLOCK(f);
	if (fd < 0) { errno = EBADF; return -1; }
	return fd;
}

int getc_unlocked(FILE *f) { return getc_unlocked_(f); }
int getchar_unlocked(void) { return getc_unlocked_(stdin); }
int putc_unlocked(int c, FILE *f) { return putc_unlocked_(c, f); }
int putchar_unlocked(int c) { return putc_unlocked_(c, stdout); }
