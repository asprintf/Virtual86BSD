#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

#define AT_EMPTY_PATH_ 0x1000

int __openat_flags(int dirfd, const char *path, int flags, mode_t mode)
{
	long r = __syscall_cp4(SYS_openat, dirfd, path, flags & ~O_CLOFORK, mode);
	if (r >= 0 && (flags & O_PATH)) {
		/* O_EXEC and O_SEARCH are Linux's O_PATH, which opens without
		 * looking at the permissions: POSIX wants EACCES when execute
		 * (search) permission is denied.  By the effective IDs, on the
		 * file opened (kernels before 5.8: not checked). */
		long a = __syscall4(SYS_faccessat2, r, "", X_OK, AT_EMPTY_PATH_ | AT_EACCESS);
		if (a < 0 && a != -ENOSYS) {
			__syscall1(SYS_close, r);
			r = a;
		}
	}
	if (r >= 0 && (flags & O_CLOFORK) && __clofork_set((int)r, 1) < 0) {
		__syscall1(SYS_close, r);
		return -1;
	}
	return (int)__syscall_ret((unsigned long)r);
}
