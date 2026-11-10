#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

int pipe2(int fd[2], int flags)
{
	long r;
	if (flags & ~(O_CLOEXEC | O_CLOFORK | O_NONBLOCK)) { errno = EINVAL; return -1; }
	if ((flags & O_CLOFORK) && __clofork_native())
		return (int)__sys2(SYS_pipe2, fd, flags);
	r = __syscall2(SYS_pipe2, fd, flags & ~O_CLOFORK);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	if ((flags & O_CLOFORK) && (__clofork_set(fd[0], 1) < 0 || __clofork_set(fd[1], 1) < 0)) {
		__clofork_set(fd[0], 0);
		__syscall1(SYS_close, fd[0]);
		__syscall1(SYS_close, fd[1]);
		return -1;
	}
	return 0;
}

int pipe(int fd[2]) { return pipe2(fd, 0); }
