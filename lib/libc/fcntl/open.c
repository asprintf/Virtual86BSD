#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"
#include "clofork.h"

int __openat_flags(int, const char *, int, mode_t);

int open(const char *path, int flags, ...)
{
	mode_t mode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return __openat_flags(AT_FDCWD, path, flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...)
{
	mode_t mode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return __openat_flags(dirfd, path, flags, mode);
}

int creat(const char *path, mode_t mode)
{
	return __openat_flags(AT_FDCWD, path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}
