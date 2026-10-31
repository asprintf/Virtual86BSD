#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

int isatty(int fd)
{
	struct termios t;
	long r = __syscall3(SYS_ioctl, fd, 0x5401 /* TCGETS */, &t);
	if (r == 0) return 1;
	errno = r == -EBADF ? EBADF : ENOTTY;
	return 0;
}

int ttyname_r(int fd, char *buf, size_t len)
{
	char proc[32] = "/proc/self/fd/";
	char *p = proc + 14;
	char num[12], *q = num + sizeof num;
	unsigned u = (unsigned)fd;
	struct stat a, b;
	long n;

	if (!isatty(fd)) return errno;
	*--q = 0;
	do *--q = (char)('0' + u % 10); while (u /= 10);
	strcpy(p, q);
	n = __syscall4(SYS_readlinkat, -100, proc, buf, len);
	if (n < 0) return (int)-n;
	if ((size_t)n >= len) return ERANGE;
	buf[n] = 0;
	/* the link names the device unless the tty lives elsewhere (another namespace) */
	if (__syscall4(SYS_newfstatat, -100, buf, &a, 0) || __syscall2(SYS_fstat, fd, &b) ||
	    a.st_rdev != b.st_rdev || a.st_ino != b.st_ino)
		return ENODEV;
	return 0;
}

char *ttyname(int fd)
{
	static char buf[32];
	int e = ttyname_r(fd, buf, sizeof buf);
	if (e) { errno = e; return 0; }
	return buf;
}
