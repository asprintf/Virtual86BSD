#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int fstatat(int dirfd, const char *__RESTRICT path, struct stat *__RESTRICT st, int flag)
{
	if (flag & ~AT_SYMLINK_NOFOLLOW) { errno = EINVAL; return -1; }
	return (int)__sys4(SYS_newfstatat, dirfd, path, st, flag);
}
int stat(const char *__RESTRICT p, struct stat *__RESTRICT st) { return (int)__sys4(SYS_newfstatat, AT_FDCWD, p, st, 0); }
int lstat(const char *__RESTRICT p, struct stat *__RESTRICT st) { return (int)__sys4(SYS_newfstatat, AT_FDCWD, p, st, AT_SYMLINK_NOFOLLOW); }
int fstat(int fd, struct stat *st) { return (int)__sys2(SYS_fstat, fd, st); }
