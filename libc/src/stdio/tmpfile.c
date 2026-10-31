/* tmpfile() and tmpnam(): names from getrandom() bytes; tmpfile()'s file
 * is unlinked at once. */
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "stdio_impl.h"
#include "syscall.h"

int __randname(char *template6)   /* fills 6 characters; 0 on success */
{
	static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	unsigned char r[6];
	int i;
	if (__syscall3(SYS_getrandom, r, sizeof r, 0) != sizeof r) return -1;
	for (i = 0; i < 6; i++) template6[i] = chars[r[i] % 62];
	return 0;
}

FILE *tmpfile(void)
{
	char name[] = "/tmp/tmpfile_XXXXXX";
	int tries, fd;
	for (tries = 0; tries < 100; tries++) {
		if (__randname(name + 13)) break;
		fd = (int)__syscall4(SYS_openat, -100, name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			FILE *f;
			__syscall3(SYS_unlinkat, -100, name, 0);
			f = __fdopen(fd, "w+");
			if (!f) __syscall1(SYS_close, fd);
			return f;
		}
		if (fd != -EEXIST) { errno = -fd; return 0; }
	}
	return 0;
}

char *tmpnam(char *buf)
{
	static char internal[L_tmpnam];
	char name[] = "/tmp/tmpnam_XXXXXX";
	struct { long pad[18]; } st;
	int tries;
	for (tries = 0; tries < 100; tries++) {
		if (__randname(name + 12)) break;
		if (__syscall4(SYS_newfstatat, -100, name, &st, 0x100) == -ENOENT) {
			char *d = buf ? buf : internal;
			memcpy(d, name, sizeof name);
			return d;
		}
	}
	return 0;
}
