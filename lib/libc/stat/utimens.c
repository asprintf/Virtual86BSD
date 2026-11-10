#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int utimensat(int fd, const char *p, const struct timespec t[2], int flag)
{
	return (int)__sys4(SYS_utimensat, fd, p, t, flag);
}

int futimens(int fd, const struct timespec t[2])
{
	return (int)__sys4(SYS_utimensat, fd, 0, t, 0);
}

int utimes(const char *p, const struct timeval tv[2])
{
	struct timespec ts[2];
	if (!tv) return utimensat(AT_FDCWD, p, 0, 0);
	if (tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 ||
	    tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000) { errno = EINVAL; return -1; }
	ts[0].tv_sec = tv[0].tv_sec; ts[0].tv_nsec = tv[0].tv_usec * 1000;
	ts[1].tv_sec = tv[1].tv_sec; ts[1].tv_nsec = tv[1].tv_usec * 1000;
	return utimensat(AT_FDCWD, p, ts, 0);
}
