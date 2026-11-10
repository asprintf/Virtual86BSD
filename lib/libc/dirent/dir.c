/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dir_impl.h"
#include "../internal/syscall.h"
#include "../internal/lock.h"

ssize_t posix_getdents(int fd, void *buf, size_t n, int flags)
{
	if (flags) { errno = EINVAL; return -1; }
	if (n > 0x7ffff000) n = 0x7ffff000;
	return __sys3(SYS_getdents64, fd, buf, n);
}

DIR *fdopendir(int fd)
{
	struct stat st;
	DIR *d;
	int fl;
	if (fstat(fd, &st) < 0) return 0;
	if ((fl = fcntl(fd, F_GETFL)) < 0) return 0;
	if ((fl & O_ACCMODE) == O_WRONLY || (fl & O_PATH)) { errno = EBADF; return 0; }
	if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return 0; }
	if (!(d = calloc(1, sizeof *d))) return 0;
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	d->fd = fd;
	d->tell = lseek(fd, 0, SEEK_CUR);
	return d;
}

DIR *opendir(const char *name)
{
	DIR *d;
	int fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) return 0;
	if (!(d = calloc(1, sizeof *d))) { close(fd); return 0; }
	d->fd = fd;
	return d;
}

int closedir(DIR *d)
{
	int r = close(d->fd);
	free(d);
	return r;
}

int dirfd(DIR *d) { return d->fd; }

static struct dirent *next(DIR *d)
{
	struct dirent *de;
	if (d->pos >= d->end) {
		long n = __syscall3(SYS_getdents64, d->fd, d->buf, sizeof d->buf);
		if (n <= 0) {
			/* a removed directory reads as empty */
			if (n < 0 && n != -ENOENT) errno = (int)-n;
			return 0;
		}
		d->end = (size_t)n;
		d->pos = 0;
	}
	de = (struct dirent *)(d->buf + d->pos);
	d->pos += de->__d_reclen;
	d->tell = de->__d_off;
	return de;
}

struct dirent *readdir(DIR *d)
{
	struct dirent *de;
	LOCK(&d->lock);
	de = next(d);
	UNLOCK(&d->lock);
	return de;
}

int readdir_r(DIR *__RESTRICT d, struct dirent *__RESTRICT ent, struct dirent **__RESTRICT res)
{
	struct dirent *de;
	int e = errno, r;
	LOCK(&d->lock);
	errno = 0;
	de = next(d);
	r = errno;
	errno = e;
	if (de) memcpy(ent, de, de->__d_reclen < sizeof *ent ? de->__d_reclen : sizeof *ent);
	UNLOCK(&d->lock);
	if (r) { *res = 0; return r; }
	*res = de ? ent : 0;
	return 0;
}

void rewinddir(DIR *d)
{
	LOCK(&d->lock);
	lseek(d->fd, 0, SEEK_SET);
	d->pos = d->end = 0;
	d->tell = 0;
	UNLOCK(&d->lock);
}
