#include <unistd.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

/* Linux always releases the descriptor, even when interrupted; report
 * that as EINPROGRESS, as POSIX.1-2024 allows (POSIX_CLOSE_RESTART is 0). */
static int do_close(int fd)
{
	long r;
	if (__clofork_any) __clofork_set(fd, 0);
	r = __syscall_cp1(SYS_close, fd);
	if (r == -EINTR) r = -EINPROGRESS;
	return (int)__syscall_ret((unsigned long)r);
}

int close(int fd) { return do_close(fd); }

int posix_close(int fd, int flag)
{
	int r = do_close(fd);
	if (flag & ~POSIX_CLOSE_RESTART && r == 0) { errno = EINVAL; return -1; }
	return r;
}
