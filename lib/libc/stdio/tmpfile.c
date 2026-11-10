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
