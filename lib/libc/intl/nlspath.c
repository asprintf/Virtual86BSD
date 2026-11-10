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

#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "intl_impl.h"
#include "syscall.h"

/* parts of language[_territory][.codeset][@modifier] */
static void part(const char *loc, int which, const char **s, size_t *l)
{
	const char *u = strchr(loc, '_'), *d = strchr(loc, '.'), *a = strchr(loc, '@'), *e = loc + strlen(loc);
	const char *end = a ? a : e;
	if (d && d > end) d = 0;
	if (u && u > (d ? d : end)) u = 0;
	switch (which) {
	case 'l': *s = loc; *l = (size_t)((u ? u : d ? d : end) - loc); break;
	case 't': if (u) { *s = u + 1; *l = (size_t)((d ? d : end) - u - 1); } else { *s = ""; *l = 0; } break;
	case 'c': if (d) { *s = d + 1; *l = (size_t)(end - d - 1); } else { *s = ""; *l = 0; } break;
	}
}

int __nlspath(const char *spec, const char *name, const char *loc, int (*f)(const char *, void *), void *arg)
{
	char path[PATH_MAX];
	const char *p = spec;
	int r = 0;
	while (*p) {
		size_t k = 0;
		int ok = 1;
		for (; *p && *p != ':'; p++) {
			const char *s = 0;
			size_t l = 0;
			if (*p == '%' && p[1]) {
				p++;
				switch (*p) {
				case 'N': s = name; l = strlen(name); break;
				case 'L': s = loc; l = strlen(loc); break;
				case 'l': case 't': case 'c': part(loc, *p, &s, &l); break;
				case '%': s = "%"; l = 1; break;
				default: s = p - 1; l = 2; break;
				}
			} else { s = p; l = 1; }
			if (k + l >= sizeof path) { ok = 0; continue; }
			memcpy(path + k, s, l);
			k += l;
		}
		path[k] = 0;
		if (ok && k && (r = f(path, arg))) return r;
		if (*p == ':') p++;
	}
	return r;
}

void *__map_file(const char *path, size_t *size)
{
	struct stat st;
	long fd = __syscall4(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	void *m;
	if (fd < 0) return 0;
	if (__syscall2(SYS_fstat, fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > (1L << 30)) {
		__syscall1(SYS_close, fd);
		return 0;
	}
	m = (void *)__syscall6x(SYS_mmap, 0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	__syscall1(SYS_close, fd);
	if ((unsigned long)m > -4096UL) return 0;
	*size = (size_t)st.st_size;
	return m;
}
