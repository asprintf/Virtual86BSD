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
