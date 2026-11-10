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
