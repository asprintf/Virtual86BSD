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

/* setenv, unsetenv, putenv.
 * The environment array is reallocated by the library once it changes;
 * strings the library made for setenv() are tracked so that replacing or
 * removing them frees them (strings given to putenv() belong to the
 * application). */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libc.h"
#include "lock.h"

static volatile int lock;
static char **own_env;        /* the array, if the library allocated it */
static char **made; static size_t nmade, capmade;   /* strings from setenv */

static void forget(char *s)
{
	size_t i;
	for (i = 0; i < nmade; i++)
		if (made[i] == s) { free(s); made[i] = made[--nmade]; return; }
}

static int remember(char *s)
{
	if (nmade == capmade) {
		size_t n = capmade ? 2 * capmade : 16;
		char **m = realloc(made, n * sizeof *m);
		if (!m) return -1;
		made = m; capmade = n;
	}
	made[nmade++] = s;
	return 0;
}

/* Put s (NAME=value) in the environment, replacing the entry for the
 * same name (whose length is l). */
static int put(char *s, size_t l, int is_made)
{
	size_t n = 0;
	char **e = __environ, **ne;
	if (e) for (; e[n]; n++)
		if (!strncmp(e[n], s, l) && e[n][l] == '=') {
			char *old = e[n];
			e[n] = s;
			forget(old);
			return 0;
		}
	ne = own_env == __environ && own_env ? realloc(own_env, (n + 2) * sizeof *ne) : malloc((n + 2) * sizeof *ne);
	if (!ne) { if (is_made) free(s); errno = ENOMEM; return -1; }
	if (__environ != own_env && n) memcpy(ne, __environ, n * sizeof *ne);
	ne[n] = s;
	ne[n + 1] = 0;
	__environ = own_env = ne;
	return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
	size_t l, lv;
	char *s;
	int r;
	if (!name || !(l = strcspn(name, "=")) || name[l]) { errno = EINVAL; return -1; }
	LOCK(&lock);
	if (!overwrite && getenv(name)) { UNLOCK(&lock); return 0; }
	lv = strlen(value);
	s = malloc(l + lv + 2);
	if (!s || remember(s)) { free(s); UNLOCK(&lock); errno = ENOMEM; return -1; }
	memcpy(s, name, l);
	s[l] = '=';
	memcpy(s + l + 1, value, lv + 1);
	r = put(s, l, 1);
	if (r) nmade--;
	UNLOCK(&lock);
	return r;
}

int putenv(char *s)
{
	size_t l = strcspn(s, "=");
	int r;
	if (!s[l] || !l) {   /* no '=': remove the variable, as other systems do */
		return unsetenv(s);
	}
	LOCK(&lock);
	r = put(s, l, 0);
	UNLOCK(&lock);
	return r;
}

int unsetenv(const char *name)
{
	size_t l;
	char **e, **d;
	if (!name || !(l = strcspn(name, "=")) || name[l]) { errno = EINVAL; return -1; }
	LOCK(&lock);
	if (__environ) {
		for (e = d = __environ; *e; e++) {
			if (!strncmp(*e, name, l) && (*e)[l] == '=') forget(*e);
			else *d++ = *e;
		}
		*d = 0;
	}
	UNLOCK(&lock);
	return 0;
}
