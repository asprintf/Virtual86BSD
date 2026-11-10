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

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pwf.h"

static char *put(char **p, const char *s)
{
	char *r = *p;
	size_t l = strlen(s) + 1;
	memcpy(r, s, l);
	*p += l;
	return r;
}

int __getpw_r(const char *name, uid_t uid, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
	FILE *f;
	char *line = 0, *p;
	size_t n = 0, need;
	struct passwd t;
	int e = errno, r = 0;
	*res = 0;
	if (!(f = fopen(PASSWD_FILE, "re"))) {
		r = errno == ENOENT ? 0 : errno;
		errno = e;
		return r;
	}
	for (;;) {
		errno = 0;
		if (getline(&line, &n, f) < 0) { r = ferror(f) ? errno : 0; break; }
		if (__parse_pw(line, &t)) continue;
		if (name ? strcmp(name, t.pw_name) : uid != t.pw_uid) continue;
		need = strlen(t.pw_name) + strlen(t.pw_passwd) + strlen(t.pw_gecos)
		     + strlen(t.pw_dir) + strlen(t.pw_shell) + 5;
		if (need > size) { r = ERANGE; break; }
		p = buf;
		pw->pw_name = put(&p, t.pw_name);
		pw->pw_passwd = put(&p, t.pw_passwd);
		pw->pw_uid = t.pw_uid;
		pw->pw_gid = t.pw_gid;
		pw->pw_gecos = put(&p, t.pw_gecos);
		pw->pw_dir = put(&p, t.pw_dir);
		pw->pw_shell = put(&p, t.pw_shell);
		*res = pw;
		break;
	}
	free(line);
	fclose(f);
	errno = e;
	return r;
}

int getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
	return __getpw_r(name, 0, pw, buf, size, res);
}

int getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
	return __getpw_r(0, uid, pw, buf, size, res);
}

static struct passwd *getpw(const char *name, uid_t uid)
{
	static struct passwd pw;
	static char *buf;
	static size_t size;
	struct passwd *res;
	char *nb;
	int r;
	while ((r = __getpw_r(name, uid, &pw, buf, size, &res)) == ERANGE) {
		if (!(nb = realloc(buf, size ? 2 * size : 256))) return 0;
		buf = nb;
		size = size ? 2 * size : 256;
	}
	if (r) errno = r;
	return res;
}

struct passwd *getpwnam(const char *name) { return getpw(name, 0); }
struct passwd *getpwuid(uid_t uid) { return getpw(0, uid); }
