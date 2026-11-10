/* Creating streams on file descriptors.  ISO C fopen() and POSIX
 * fdopen() share this; it uses only reserved names. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "stdio_impl.h"
#include "syscall.h"

int __fmodeflags(const char *mode)
{
	int flags;
	if (strchr(mode, '+')) flags = O_RDWR;
	else if (*mode == 'r') flags = O_RDONLY;
	else flags = O_WRONLY;
	if (strchr(mode, 'x')) flags |= O_EXCL;
	if (strchr(mode, 'e')) flags |= O_CLOEXEC;
	if (*mode != 'r') flags |= O_CREAT;
	if (*mode == 'w') flags |= O_TRUNC;
	if (*mode == 'a') flags |= O_APPEND;
	return flags;
}

FILE *__fdopen(int fd, const char *mode)
{
	FILE *f;
	if (!strchr("rwa", *mode)) { errno = EINVAL; return 0; }
	if (!(f = malloc(sizeof *f + UNGET + BUFSIZ))) return 0;
	memset(f, 0, sizeof *f);
	if (!strchr(mode, '+')) f->flags = *mode == 'r' ? F_NOWR : F_NORD;
	if (strchr(mode, 'e')) __syscall3(SYS_fcntl, fd, 2 /* F_SETFD */, 1 /* FD_CLOEXEC */);
	if (*mode == 'a') {
		long fl = __syscall2(SYS_fcntl, fd, 3 /* F_GETFL */);
		if (!(fl & O_APPEND)) __syscall3(SYS_fcntl, fd, 4 /* F_SETFL */, fl | O_APPEND);
		f->flags |= F_APP;
		/* as glibc: "a" (not "a+") starts at the end, so ftell tells the size */
		if (!strchr(mode, '+')) __syscall3(SYS_lseek, fd, 0, SEEK_END);
	}
	f->fd = fd;
	f->buf = (unsigned char *)f + sizeof *f + UNGET;
	f->buf_size = BUFSIZ;
	f->lbf = -1;
	f->flags |= F_SVB;
	if (!(f->flags & F_NOWR)) {
		struct { unsigned short r, c, x, y; } ws;
		if (!__syscall3(SYS_ioctl, fd, 0x5413 /* TIOCGWINSZ */, &ws)) f->lbf = '\n';
	}
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;
	return __ofl_add(f);
}
