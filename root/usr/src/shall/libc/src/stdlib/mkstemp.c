#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int __randname(char *);
int __openat_flags(int, const char *, int, mode_t);

/* the last six characters must be XXXXXX */
static char *xs(char *t)
{
	size_t l = strlen(t);
	if (l < 6 || memcmp(t + l - 6, "XXXXXX", 6)) { errno = EINVAL; return 0; }
	return t + l - 6;
}

int mkostemp(char *t, int flags)
{
	char *x = xs(t);
	int tries, fd;
	if (!x) return -1;
	if (flags & ~(O_APPEND | O_CLOEXEC | O_CLOFORK | O_DSYNC | O_RSYNC | O_SYNC)) { errno = EINVAL; return -1; }
	for (tries = 0; tries < 1000; tries++) {
		if (__randname(x)) break;
		fd = __openat_flags(AT_FDCWD, t, flags | O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) return fd;
		if (errno != EEXIST) break;
	}
	memcpy(x, "XXXXXX", 6);
	return -1;
}

int mkstemp(char *t) { return mkostemp(t, 0); }

char *mkdtemp(char *t)
{
	char *x = xs(t);
	int tries;
	if (!x) return 0;
	for (tries = 0; tries < 1000; tries++) {
		if (__randname(x)) break;
		if (!mkdir(t, 0700)) return t;
		if (errno != EEXIST) break;
	}
	memcpy(x, "XXXXXX", 6);
	return 0;
}
