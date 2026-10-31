#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "stdio_impl.h"
#include "syscall.h"

int __openat_flags(int, const char *, int, mode_t);

FILE *fopen(const char *__RESTRICT path, const char *__RESTRICT mode)
{
	FILE *f;
	int fd, flags;
	if (!strchr("rwa", *mode)) { errno = EINVAL; return 0; }
	flags = __fmodeflags(mode);
	fd = __openat_flags(AT_FDCWD, path, flags, 0666);
	if (fd < 0) return 0;
	if (flags & O_CLOEXEC) __syscall3(SYS_fcntl, fd, 2, 1);
	f = __fdopen(fd, mode);
	if (f) return f;
	__syscall1(SYS_close, fd);
	return 0;
}
