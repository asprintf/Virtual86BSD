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

#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Pathname expansion (XCU 2.14.3). */

struct g {
	int flags;
	int (*errf)(const char *, int);
	char **v;                   /* new paths */
	size_t n, cap;
	int err;                    /* GLOB_NOSPACE or GLOB_ABORTED */
};

static int add(struct g *g, const char *path, size_t len, int mark)
{
	char *s, **nv;
	if (g->n == g->cap) {
		g->cap = g->cap ? 2 * g->cap : 16;
		if (!(nv = realloc(g->v, g->cap * sizeof *nv))) { g->err = GLOB_NOSPACE; return -1; }
		g->v = nv;
	}
	if (!(s = malloc(len + 2))) { g->err = GLOB_NOSPACE; return -1; }
	memcpy(s, path, len);
	if (mark) s[len++] = '/';
	s[len] = 0;
	g->v[g->n++] = s;
	return 0;
}

static int special(const char *p, size_t n, int flags)
{
	size_t i;
	for (i = 0; i < n; i++) {
		if (p[i] == '\\' && !(flags & GLOB_NOESCAPE)) { i++; continue; }
		if (p[i] == '*' || p[i] == '?' || p[i] == '[') return 1;
	}
	return 0;
}

static int isdir(const char *path)
{
	struct stat st;
	return !stat(path, &st) && S_ISDIR(st.st_mode);
}

/* buf holds the path so far (len bytes, ending with '/' unless empty);
 * pat is the rest of the pattern */
static void expand(struct g *g, char *buf, size_t len, size_t cap, const char *pat)
{
	const char *e;
	size_t cl, i;
	struct stat st;
	while (*pat == '/') {           /* slashes are copied */
		if (len + 1 >= cap) { g->err = GLOB_NOSPACE; return; }
		buf[len++] = *pat++;
	}
	buf[len] = 0;
	if (!*pat) {
		/* the end, after a slash (or an empty pattern) */
		if (len && !lstat(buf, &st) && (buf[len - 1] != '/' || isdir(buf))) add(g, buf, len, 0);
		return;
	}
	e = strchr(pat, '/');
	cl = e ? (size_t)(e - pat) : strlen(pat);
	if (!special(pat, cl, g->flags)) {
		/* a literal component: copy it without its escapes */
		for (i = 0; i < cl; i++) {
			if (pat[i] == '\\' && !(g->flags & GLOB_NOESCAPE) && i + 1 < cl) i++;
			if (len + 1 >= cap) { g->err = GLOB_NOSPACE; return; }
			buf[len++] = pat[i];
		}
		buf[len] = 0;
		if (!e) {
			if (!lstat(buf, &st)) add(g, buf, len, (g->flags & GLOB_MARK) && isdir(buf));
			return;
		}
		expand(g, buf, len, cap, pat + cl);
		return;
	}
	{
		char *comp = malloc(cl + 1);
		DIR *d;
		struct dirent *de;
		int fl = FNM_PERIOD | (g->flags & GLOB_NOESCAPE ? FNM_NOESCAPE : 0);
		if (!comp) { g->err = GLOB_NOSPACE; return; }
		memcpy(comp, pat, cl);
		comp[cl] = 0;
		buf[len] = 0;
		if (!(d = opendir(len ? buf : "."))) {
			free(comp);
			if (errno != ENOENT && errno != ENOTDIR) {
				if ((g->errf && g->errf(len ? buf : ".", errno)) || (g->flags & GLOB_ERR))
					g->err = GLOB_ABORTED;
			}
			return;
		}
		while (!g->err && (errno = 0, de = readdir(d))) {
			size_t nl = strlen(de->d_name);
			if (fnmatch(comp, de->d_name, fl)) continue;
			if (len + nl + 2 >= cap) { g->err = GLOB_NOSPACE; break; }
			memcpy(buf + len, de->d_name, nl + 1);
			if (!e) add(g, buf, len + nl, (g->flags & GLOB_MARK) && isdir(buf));
			else if (e[strspn(e, "/")] == 0) {
				/* trailing slashes: directories only */
				if (isdir(buf)) {
					memcpy(buf + len + nl, e, strlen(e) + 1);
					add(g, buf, len + nl + strlen(e), 0);
				}
			} else
				expand(g, buf, len + nl, cap, e);
		}
		if (!g->err && errno && ((g->errf && g->errf(len ? buf : ".", errno)) || (g->flags & GLOB_ERR)))
			g->err = GLOB_ABORTED;
		closedir(d);
		free(comp);
	}
}

static int cmp(const void *a, const void *b)
{
	return strcoll(*(char *const *)a, *(char *const *)b);
}

int glob(const char *__RESTRICT pat, int flags, int (*errf)(const char *, int), glob_t *__RESTRICT gl)
{
	struct g g = { 0 };
	size_t cap = 4096 + strlen(pat), offs, old, i;
	char *buf, **nv;
	if (!(flags & GLOB_APPEND)) {
		gl->gl_pathc = 0;
		gl->gl_pathv = 0;
		if (!(flags & GLOB_DOOFFS)) gl->gl_offs = 0;
	}
	offs = flags & GLOB_DOOFFS ? gl->gl_offs : 0;
	g.flags = flags;
	g.errf = errf;
	if (!(buf = malloc(cap))) return GLOB_NOSPACE;
	if (*pat) expand(&g, buf, 0, cap, pat);
	free(buf);
	if (g.err == GLOB_NOSPACE) goto fail;
	if (!g.n && !g.err) {
		if (!(flags & GLOB_NOCHECK)) { free(g.v); return GLOB_NOMATCH; }
		if (add(&g, pat, strlen(pat), 0)) goto fail;
	}
	if (!(flags & GLOB_NOSORT) && g.n > 1) qsort(g.v, g.n, sizeof *g.v, cmp);
	old = gl->gl_pathv ? gl->gl_pathc : 0;
	if (!(nv = realloc(gl->gl_pathv, (offs + old + g.n + 1) * sizeof *nv))) goto fail;
	if (!gl->gl_pathv) for (i = 0; i < offs; i++) nv[i] = 0;
	gl->gl_pathv = nv;
	memcpy(nv + offs + old, g.v, g.n * sizeof *nv);
	gl->gl_pathc = old + g.n;
	nv[offs + gl->gl_pathc] = 0;
	free(g.v);
	return g.err;
fail:
	for (i = 0; i < g.n; i++) free(g.v[i]);
	free(g.v);
	return GLOB_NOSPACE;
}

void globfree(glob_t *gl)
{
	size_t i, offs = gl->gl_offs;
	if (!gl->gl_pathv) return;
	for (i = 0; i < gl->gl_pathc; i++) free(gl->gl_pathv[offs + i]);
	free(gl->gl_pathv);
	gl->gl_pathv = 0;
	gl->gl_pathc = 0;
}
