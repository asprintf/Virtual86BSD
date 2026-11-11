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

#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

int uname(struct utsname *u) { return (int)__sys1(SYS_uname, u); }

int gethostname(char *name, size_t len)
{
	struct utsname u;
	size_t l;
	if (uname(&u)) return -1;
	l = strlen(u.nodename);
	if (l >= len) { errno = ENAMETOOLONG; return -1; }
	memcpy(name, u.nodename, l + 1);
	return 0;
}

int sethostname(const char *name, size_t len)
{
	return (int)__sys2(SYS_sethostname, name, len);
}

long gethostid(void)
{
	/* /etc/hostid (4 bytes, native order) if present, else 0 */
	unsigned int id = 0;
	long fd = __syscall4(SYS_openat, -100, "/etc/hostid", 0, 0);
	if (fd >= 0) {
		if (__syscall3(SYS_read, fd, &id, sizeof id) != sizeof id) id = 0;
		__syscall1(SYS_close, fd);
	}
	return (long)(int)id;
}
