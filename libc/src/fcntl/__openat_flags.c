#include <fcntl.h>
#include "syscall.h"
#include "clofork.h"

int __openat_flags(int dirfd, const char *path, int flags, mode_t mode)
{
	long r = __syscall_cp4(SYS_openat, dirfd, path, flags & ~O_CLOFORK, mode);
	if (r >= 0 && (flags & O_CLOFORK) && __clofork_set((int)r, 1) < 0) {
		__syscall1(SYS_close, r);
		return -1;
	}
	return (int)__syscall_ret((unsigned long)r);
}
