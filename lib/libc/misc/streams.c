/*
 * Copyright (c) 2026, asprintf
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

/* STREAMS (<stropts.h>, SUSv2 to SUSv4): no file is a STREAM */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stropts.h>
#include <sys/stat.h>

/* the access mode of fd: O_RDONLY, O_WRONLY, O_RDWR, or -1 (EBADF) */
static int mode(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	return fl < 0 ? -1 : fl & O_ACCMODE;
}

int isastream(int fd)
{
	return mode(fd) < 0 ? -1 : 0;
}

int getmsg(int fd, struct strbuf *__RESTRICT ctl, struct strbuf *__RESTRICT data, int *__RESTRICT flags)
{
	int m = mode(fd);
	(void)ctl; (void)data; (void)flags;
	if (m < 0 || m == O_WRONLY) { errno = EBADF; return -1; }
	errno = ENOSTR;
	return -1;
}

int getpmsg(int fd, struct strbuf *__RESTRICT ctl, struct strbuf *__RESTRICT data, int *__RESTRICT band, int *__RESTRICT flags)
{
	(void)band;
	return getmsg(fd, ctl, data, flags);
}

int putmsg(int fd, const struct strbuf *ctl, const struct strbuf *data, int flags)
{
	int m = mode(fd);
	(void)ctl; (void)data; (void)flags;
	if (m < 0 || m == O_RDONLY) { errno = EBADF; return -1; }
	errno = ENOSTR;
	return -1;
}

int putpmsg(int fd, const struct strbuf *ctl, const struct strbuf *data, int band, int flags)
{
	(void)band;
	return putmsg(fd, ctl, data, flags);
}

int fattach(int fd, const char *path)
{
	(void)path;
	if (mode(fd) < 0) { errno = EBADF; return -1; }
	errno = EINVAL;
	return -1;
}

int fdetach(const char *path)
{
	struct stat st;
	if (stat(path, &st) < 0) return -1;
	errno = EINVAL;
	return -1;
}
