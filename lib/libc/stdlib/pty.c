#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

#define TIOCGPTN   0x80045430
#define TIOCSPTLCK 0x40045431

int __openat_flags(int, const char *, int, mode_t);

int posix_openpt(int flags)
{
	if (flags & ~(O_RDWR | O_NOCTTY | O_CLOEXEC | O_CLOFORK)) { errno = EINVAL; return -1; }
	return __openat_flags(AT_FDCWD, "/dev/ptmx", flags, 0);
}

int grantpt(int fd)
{
	/* devpts gives the slave the right owner and mode already */
	int n;
	return (int)__sys3(SYS_ioctl, fd, TIOCGPTN, &n) < 0 ? -1 : 0;
}

int unlockpt(int fd)
{
	int unlock = 0;
	return (int)__sys3(SYS_ioctl, fd, TIOCSPTLCK, &unlock);
}

int ptsname_r(int fd, char *buf, size_t len)
{
	unsigned n;
	char tmp[32], num[12], *p = num + sizeof num;
	long r = __syscall3(SYS_ioctl, fd, TIOCGPTN, &n);
	if (r) return r == -ENOTTY ? EINVAL : (int)-r;
	*--p = 0;
	do *--p = (char)('0' + n % 10); while (n /= 10);
	strcpy(tmp, "/dev/pts/");
	strcat(tmp, p);
	if (strlen(tmp) >= len) return ERANGE;
	strcpy(buf, tmp);
	return 0;
}

char *ptsname(int fd)
{
	static char buf[32];
	int e = ptsname_r(fd, buf, sizeof buf);
	if (e) { errno = e; return 0; }
	return buf;
}
