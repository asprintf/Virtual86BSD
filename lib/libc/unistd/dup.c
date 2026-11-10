#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

int dup(int fd) { return (int)__sys1(SYS_dup, fd); }

int dup3(int old, int new, int flags)
{
	long r;
	if (flags & ~(O_CLOEXEC | O_CLOFORK)) { errno = EINVAL; return -1; }
	if (old == new) { errno = EINVAL; return -1; }
	if (__clofork_native()) {
		while ((r = __syscall3(SYS_dup3, old, new, flags)) == -EBUSY);
		return (int)__syscall_ret((unsigned long)r);
	}
	while ((r = __syscall3(SYS_dup3, old, new, flags & O_CLOEXEC)) == -EBUSY);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	if ((flags & O_CLOFORK) || __clofork_any) {
		if (__clofork_set(new, (flags & O_CLOFORK) != 0) < 0) {
			__syscall1(SYS_close, new);
			return -1;
		}
	}
	return new;
}

int dup2(int old, int new)
{
	long r;
	if (old == new) {
		/* valid descriptor: return it unchanged */
		r = __syscall2(SYS_fcntl, old, F_GETFD);
		return r < 0 ? (int)__syscall_ret((unsigned long)r) : new;
	}
	return dup3(old, new, 0);
}
