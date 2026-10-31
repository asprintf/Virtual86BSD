#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <errno.h>
#include "syscall.h"

int getentropy(void *buf, size_t len)
{
	char *p = buf;
	if (len > 256) { errno = EINVAL; return -1; }
	while (len) {
		long r = __syscall3(SYS_getrandom, p, len, 0);
		if (r == -EINTR) continue;
		if (r < 0) return (int)__syscall_ret((unsigned long)r);
		p += r;
		len -= (size_t)r;
	}
	return 0;
}

int nice(int inc)
{
	long prio = 20 - __syscall2(SYS_getpriority, PRIO_PROCESS, 0); /* kernel returns 20-nice */
	if (inc > 40) inc = 40;
	if (inc < -40) inc = -40;
	prio += inc;
	if (prio > 19) prio = 19;
	if (prio < -20) prio = -20;
	if (__syscall3(SYS_setpriority, PRIO_PROCESS, 0, prio) < 0) {
		errno = EPERM;  /* POSIX: EPERM for a refused increase */
		return -1;
	}
	return (int)prio;
}

int lockf(int fd, int op, off_t size)
{
	struct flock l = { F_WRLCK, SEEK_CUR, 0, size, 0 };
	switch (op) {
	case F_TEST:
		l.l_type = F_RDLCK;
		if (fcntl(fd, F_GETLK, &l) < 0) return -1;
		if (l.l_type == F_UNLCK || l.l_pid == getpid()) return 0;
		errno = EACCES;
		return -1;
	case F_ULOCK: l.l_type = F_UNLCK; /* fall through */
	case F_TLOCK: return fcntl(fd, F_SETLK, &l);
	case F_LOCK:  return fcntl(fd, F_SETLKW, &l);
	}
	errno = EINVAL;
	return -1;
}

void swab(const void *__RESTRICT src, void *__RESTRICT dst, ssize_t n)
{
	const char *s = src;
	char *d = dst;
	for (; n > 1; n -= 2, s += 2, d += 2) { d[0] = s[1]; d[1] = s[0]; }
}
