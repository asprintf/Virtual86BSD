/* getcwd: the kernel's answer; for a directory deeper than the kernel
 * reports (ENAMETOOLONG beyond a page), the name is found by walking up
 * through "..", looking in each parent for the entry that is the child
 * (as glibc does). */
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "syscall.h"

char *__strdup_internal(const char *);

/* the path of the working directory into buf[0..n), built from the end */
static char *walk(char *buf, size_t n)
{
	struct stat cur, par, st;
	char *p = buf + n;
	int fd, pfd, e;
	DIR *d;
	struct dirent *de;

	if ((fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0)
		return 0;
	if (fstat(fd, &cur) < 0)
		goto fail;
	*--p = 0;
	for (;;) {
		if ((pfd = openat(fd, "..", O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0)
			goto fail;
		if (fstat(pfd, &par) < 0) {
			close(pfd);
			goto fail;
		}
		if (par.st_dev == cur.st_dev && par.st_ino == cur.st_ino) {
			close(pfd);                    /* the root */
			break;
		}
		close(fd);
		if ((fd = dup(pfd)) < 0 || !(d = fdopendir(pfd))) {
			close(pfd);
			goto fail;
		}
		for (;;) {
			errno = 0;
			if (!(de = readdir(d))) {
				if (!errno)
					errno = ENOENT;        /* removed meanwhile */
				break;
			}
			if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
				continue;
			/* d_ino alone misleads at mount points: compare what is there */
			if (par.st_dev == cur.st_dev && de->d_ino != cur.st_ino)
				continue;
			if (fstatat(fd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0
			    && st.st_dev == cur.st_dev && st.st_ino == cur.st_ino)
				break;
		}
		if (!de) {
			e = errno;
			closedir(d);
			errno = e;
			goto fail;
		}
		{
			size_t l = strlen(de->d_name);
			if ((size_t)(p - buf) < l + 2) {
				closedir(d);
				errno = ERANGE;
				goto fail;
			}
			p -= l;
			memcpy(p, de->d_name, l);
			*--p = '/';
		}
		closedir(d);
		cur = par;
	}
	close(fd);
	if (!*p)
		*--p = '/';
	memmove(buf, p, (size_t)(buf + n - p));
	return buf;
fail:
	e = errno;
	close(fd);
	errno = e;
	return 0;
}

char *getcwd(char *buf, size_t n)
{
	char tmp[4096], *b, *r;
	long k;
	size_t cap;
	if (buf && !n) {
		errno = EINVAL;
		return 0;
	}
	k = __syscall2(SYS_getcwd, buf ? buf : tmp, buf ? n : sizeof tmp);
	if (k >= 0) {
		b = buf ? buf : tmp;
		if (b[0] != '/') {      /* unreachable */
			errno = ENOENT;
			return 0;
		}
		return buf ? buf : __strdup_internal(tmp);
	}
	if (k != -ENAMETOOLONG || (buf && n <= sizeof tmp)) {
		/* ERANGE for a small buffer too, as the kernel said */
		errno = k == -ENAMETOOLONG ? ERANGE : (int)-k;
		return 0;
	}
	if (buf)
		return walk(buf, n);
	/* POSIX leaves a null buffer unspecified; allocate, as other systems do */
	for (cap = 2 * sizeof tmp;; cap *= 2) {
		if (!(b = malloc(cap)))
			return 0;
		if ((r = walk(b, cap)))
			return r;
		free(b);
		if (errno != ERANGE)
			return 0;
	}
}
