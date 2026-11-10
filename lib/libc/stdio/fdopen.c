#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

FILE *fdopen(int fd, const char *mode)
{
	long fl = __syscall2(SYS_fcntl, fd, F_GETFL);
	if (fl < 0) { errno = EBADF; return 0; }
	/* the mode must be allowed by the descriptor's access mode */
	if (((fl & O_ACCMODE) == O_RDONLY && (*mode != 'r' || strchr(mode, '+'))) ||
	    ((fl & O_ACCMODE) == O_WRONLY && (*mode == 'r' || strchr(mode, '+')))) {
		errno = EINVAL;
		return 0;
	}
	return __fdopen(fd, mode);
}
