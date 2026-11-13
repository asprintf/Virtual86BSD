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

/* mktemp (SUSv2, SUSv3), rand_r (POSIX.1c to 2008), valloc (SUSv2) */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __randname(char *);

char *mktemp(char *template)
{
	size_t l = strlen(template);
	struct stat st;
	int tries;
	if (l < 6 || memcmp(template + l - 6, "XXXXXX", 6)) {
		errno = EINVAL;
		*template = 0;
		return template;
	}
	for (tries = 0; tries < 1000; tries++) {
		if (__randname(template + l - 6))
			break;
		if (lstat(template, &st) < 0) {
			if (errno == ENOENT)
				return template;
			break;
		}
	}
	*template = 0;
	return template;
}

int rand_r(unsigned *seed)
{
	unsigned x = (*seed += 0x9e3779b9u);
	x ^= x >> 16;
	x *= 0x85ebca6bu;
	x ^= x >> 13;
	x *= 0xc2b2ae35u;
	x ^= x >> 16;
	return (int)(x & 0x7fffffff);
}

void *valloc(size_t size)
{
	void *p;
	int e = posix_memalign(&p, (size_t)sysconf(_SC_PAGESIZE), size ? size : 1);
	if (e) {
		errno = e;
		return 0;
	}
	return p;
}
