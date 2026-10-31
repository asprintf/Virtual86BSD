#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

/* These two return an error number instead of setting errno. */
int posix_fadvise(int fd, off_t off, off_t len, int advice)
{
	return (int)-__syscall4(SYS_fadvise64, fd, off, len, advice);
}

int posix_fallocate(int fd, off_t off, off_t len)
{
	long r;
	if (off < 0 || len <= 0) return EINVAL;
	while ((r = __syscall4(SYS_fallocate, fd, 0, off, len)) == -EINTR);
	return (int)-r;
}
