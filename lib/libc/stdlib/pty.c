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

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "syscall.h"

int posix_openpt(int flags)
{
	if (flags & ~(O_RDWR | O_NOCTTY | O_CLOEXEC | O_CLOFORK)) { errno = EINVAL; return -1; }
	return (int)__sys1(SYS_posix_openpt, flags);
}

/* the slave's permissions are the user's from the start, and it is not locked: the
 * calls only check that the descriptor is a master */
int grantpt(int fd)
{
	return (int)__sys3(SYS_ioctl, fd, TIOCPTMASTER, 0) < 0 ? -1 : 0;
}

int unlockpt(int fd)
{
	return grantpt(fd);
}

/* the device name of the master (FIODGNAME) is the slave's: pts/N */
int ptsname_r(int fd, char *buf, size_t len)
{
	struct fiodgname_arg a;
	char name[64];
	long r;
	a.len = sizeof name;
	a.buf = name;
	if ((r = __syscall3(SYS_ioctl, fd, TIOCPTMASTER, 0)) < 0) return r == -ENOTTY ? EINVAL : (int)-r;
	if ((r = __syscall3(SYS_ioctl, fd, FIODGNAME, &a)) < 0) return (int)-r;
	if (strlen(name) + 6 > len) return ERANGE;
	strcpy(buf, "/dev/");
	strcat(buf, name);
	return 0;
}

char *ptsname(int fd)
{
	static char buf[32];
	int e = ptsname_r(fd, buf, sizeof buf);
	if (e) { errno = e; return 0; }
	return buf;
}
