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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}

int scandir(const char *path, struct dirent ***res, int (*sel)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **))
{
	DIR *d = opendir(path);
	struct dirent *de, **v = 0, **nv, *c;
	size_t n = 0, cap = 0, len;
	int e = errno;
	if (!d) return -1;
	errno = 0;
	while ((de = readdir(d))) {
		if (sel && !sel(de)) continue;
		if (n == cap) {
			cap = cap ? 2 * cap : 32;
			if (cap > (size_t)0x7fffffff || !(nv = realloc(v, cap * sizeof *v))) goto fail;
			v = nv;
		}
		len = offsetof(struct dirent, d_name) + strlen(de->d_name) + 1;
		if (!(c = malloc(len < sizeof *c ? sizeof *c : len))) goto fail;
		memcpy(c, de, len);
		v[n++] = c;
	}
	if (errno) goto fail;
	closedir(d);
	errno = e;
	if (cmp && n) qsort(v, n, sizeof *v, (int (*)(const void *, const void *))cmp);
	*res = v;
	return (int)n;
fail:
	e = errno ? errno : ENOMEM;
	while (n) free(v[--n]);
	free(v);
	closedir(d);
	errno = e;
	return -1;
}
