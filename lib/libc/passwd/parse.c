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
#include "pwf.h"

static int split(char *s, char **f, int n)
{
	int i;
	size_t l = strlen(s);
	if (l && s[l - 1] == '\n') s[--l] = 0;
	if (*s == '+' || *s == '-' || *s == '#') return -1;   /* NIS markers, comments */
	for (i = 0; i < n - 1; i++) {
		f[i] = s;
		if (!(s = strchr(s, ':'))) return -1;
		*s++ = 0;
	}
	f[i] = s;
	return strchr(s, ':') ? -1 : 0;
}

static int id(const char *s, unsigned *v)
{
	unsigned long long x = 0;
	if (!*s) return -1;
	for (; *s; s++) {
		if (*s < '0' || *s > '9') return -1;
		x = x * 10 + (unsigned)(*s - '0');
		if (x > 0xffffffffULL) return -1;
	}
	*v = (unsigned)x;
	return 0;
}

int __parse_pw(char *line, struct passwd *pw)
{
	char *f[7];
	unsigned u, g;
	if (split(line, f, 7) || id(f[2], &u) || id(f[3], &g) || !*f[0]) return -1;
	pw->pw_name = f[0]; pw->pw_passwd = f[1];
	pw->pw_uid = u; pw->pw_gid = g;
	pw->pw_gecos = f[4]; pw->pw_dir = f[5]; pw->pw_shell = f[6];
	return 0;
}

int __parse_gr(char *line, struct group *gr, char ***mem, size_t *nmem)
{
	char *f[4], *s, **m;
	unsigned g;
	size_t n = 1;
	if (split(line, f, 4) || id(f[2], &g) || !*f[0]) return -1;
	gr->gr_name = f[0]; gr->gr_passwd = f[1]; gr->gr_gid = g;
	for (s = f[3]; *s; s++) n += *s == ',';
	if (n + 1 > *nmem) {
		if (!(m = realloc(*mem, (n + 1) * sizeof *m))) return -2;
		*mem = m; *nmem = n + 1;
	}
	m = *mem; n = 0;
	for (s = f[3]; *s; ) {
		m[n++] = s;
		if (!(s = strchr(s, ','))) break;
		*s++ = 0;
	}
	m[n] = 0;
	gr->gr_mem = m;
	return 0;
}
