#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

int __openat_flags(int, const char *, int, mode_t);

FILE *freopen(const char *__RESTRICT path, const char *__RESTRICT mode, FILE *__RESTRICT f)
{
	int fl = __fmodeflags(mode);
	FLOCK(f);
	__fflush_unlocked(f);
	if (!path) {
		/* change the mode of the same open file */
		if (f->fd < 0 || __syscall3(SYS_fcntl, f->fd, F_SETFL, fl & ~(O_CREAT | O_EXCL | O_TRUNC)) < 0) goto fail;
		if (fl & O_CLOEXEC) __syscall3(SYS_fcntl, f->fd, F_SETFD, FD_CLOEXEC);
	} else {
		int fd = __openat_flags(AT_FDCWD, path, fl, 0666);
		if (fd < 0) goto fail;
		if (f->fd >= 0 && fd != f->fd) {
			if (__syscall3(SYS_dup3, fd, f->fd, fl & O_CLOEXEC) < 0) { __syscall1(SYS_close, fd); goto fail; }
			__syscall1(SYS_close, fd);
		} else if (f->fd < 0) {
			f->fd = fd;
		}
	}
	f->flags = (f->flags & F_PERM) | (strchr(mode, '+') ? 0 : *mode == 'r' ? F_NOWR : F_NORD) |
	           (*mode == 'a' ? F_APP : 0);
	f->mode = 0;
	if (*mode == 'a' && !strchr(mode, '+')) __syscall3(SYS_lseek, f->fd, 0, SEEK_END);
	f->rpos = f->rend = f->wpos = f->wbase = f->wend = 0;
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;
	FUNLOCK(f);
	return f;
fail:
	FUNLOCK(f);
	fclose(f);
	return 0;
}
