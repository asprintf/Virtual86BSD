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

/* getw, putw (SUSv2), tempnam (SUSv2 to SUSv4) */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __randname(char *);

int getw(FILE *stream)
{
	int w;
	return fread(&w, sizeof w, 1, stream) == 1 ? w : EOF;
}

int putw(int w, FILE *stream)
{
	return fwrite(&w, sizeof w, 1, stream) == 1 ? 0 : EOF;
}

static int usable(const char *dir)
{
	struct stat st;
	return dir && *dir && stat(dir, &st) == 0 && S_ISDIR(st.st_mode) &&
	       access(dir, W_OK | X_OK) == 0;
}

char *tempnam(const char *dir, const char *pfx)
{
	struct stat st;
	size_t dl, pl;
	char *s;
	int tries;
	if (!usable(dir))
		dir = usable(P_tmpdir) ? P_tmpdir : "/tmp";
	dl = strlen(dir);
	while (dl > 1 && dir[dl - 1] == '/')
		dl--;
	pl = pfx ? strnlen(pfx, 5) : 0;
	if (!(s = malloc(dl + 1 + pl + 6 + 1))) {
		errno = ENOMEM;
		return 0;
	}
	memcpy(s, dir, dl);
	s[dl] = '/';
	memcpy(s + dl + 1, pfx ? pfx : "", pl);
	s[dl + 1 + pl + 6] = 0;
	for (tries = 0; tries < 1000; tries++) {
		if (__randname(s + dl + 1 + pl))
			break;
		if (lstat(s, &st) < 0 && errno == ENOENT)
			return s;
	}
	free(s);
	errno = EEXIST;
	return 0;
}
