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

#define _DEFAULT_SOURCE
#include <ftw.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef int (*fn_t)(const char *, const struct stat *, int, struct FTW *);
typedef int (*ofn_t)(const char *, const struct stat *, int);

struct anc { dev_t dev; ino_t ino; struct anc *up; };

struct w {
	fn_t fn;
	ofn_t ofn;                  /* ftw: SUSv2 to SUSv4 */
	int flags;
	dev_t dev;
	char *path;
	size_t cap;
};

static int call(struct w *w, int type, const struct stat *st, struct FTW *ftw)
{
	if (w->ofn) return w->ofn(w->path, st, type == FTW_SLN ? FTW_SL : type);
	return w->fn(w->path, st, type, ftw);
}

/* entries of a directory, read at once so that only one descriptor is
 * open at a time: names separated by null bytes */
static char *readall(DIR *d, size_t *len)
{
	struct dirent *de;
	char *b = 0, *nb;
	size_t n = 0, cap = 0, l;
	while ((errno = 0, de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		l = strlen(de->d_name) + 1;
		if (n + l > cap) {
			cap = (n + l) * 2 + 256;
			if (!(nb = realloc(b, cap))) { free(b); return 0; }
			b = nb;
		}
		memcpy(b + n, de->d_name, l);
		n += l;
	}
	if (errno) { free(b); return 0; }
	*len = n;
	return b ? b : malloc(1);
}

static int walk(struct w *w, size_t len, int base, int level, struct anc *up)
{
	struct stat st;
	struct FTW ftw;
	struct anc me;
	const char *rel = (w->flags & FTW_CHDIR) && level ? w->path + base : w->path;
	int type, r, dfd = -1, cwd = -1;
	DIR *d;
	char *names = 0, *p;
	size_t nl = 0, l;

	if (((w->flags & FTW_PHYS) ? lstat(rel, &st) : stat(rel, &st)) < 0) {
		if (!(w->flags & FTW_PHYS) && (errno == ENOENT || errno == ELOOP) && !lstat(rel, &st) && S_ISLNK(st.st_mode))
			type = FTW_SLN;
		else if (!level)
			return -1;
		else if (errno == EACCES || errno == ENOENT || errno == ELOOP || errno == ENAMETOOLONG)
			type = FTW_NS;
		else
			return -1;
	} else if (S_ISDIR(st.st_mode))
		type = FTW_D;
	else if (S_ISLNK(st.st_mode))
		type = FTW_SL;
	else
		type = FTW_F;
	if (!level) w->dev = st.st_dev;
	if ((w->flags & FTW_MOUNT) && type != FTW_NS && st.st_dev != w->dev) return 0;
	ftw.base = base;
	ftw.level = level;

	if (type == FTW_D) {
		struct anc *a;
		for (a = up; a; a = a->up)
			if (a->dev == st.st_dev && a->ino == st.st_ino) break;
		if (a) return call(w, FTW_D, &st, &ftw);   /* a cycle: not entered */
		if ((dfd = open(rel, O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0 || !(d = fdopendir(dfd))) {
			if (dfd >= 0) close(dfd);
			if (errno != EACCES) return -1;
			return call(w, FTW_DNR, &st, &ftw);
		}
		dfd = -1;
		names = readall(d, &nl);
		if (!names) { int e = errno; closedir(d); errno = e ? e : ENOMEM; return -1; }
		if (w->flags & FTW_CHDIR) {
			if ((cwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0 && errno != EACCES) {
				free(names); closedir(d); return -1;
			}
			if ((dfd = dup(dirfd(d))) < 0) { free(names); closedir(d); if (cwd >= 0) close(cwd); return -1; }
		}
		closedir(d);
		if (!(w->flags & FTW_DEPTH) && (r = call(w, FTW_D, &st, &ftw))) goto out;
		r = 0;
		if (!(w->flags & (FTW_XDEV | FTW_MOUNT)) || st.st_dev == w->dev) {
			if (dfd >= 0 && fchdir(dfd) < 0) { r = -1; goto out; }
			me.dev = st.st_dev; me.ino = st.st_ino; me.up = up;
			for (p = names; p < names + nl; p += l + 1) {
				size_t b = len + (len && w->path[len - 1] != '/');
				l = strlen(p);
				if (b + l + 1 > w->cap) {
					char *np = realloc(w->path, (b + l + 1) * 2);
					if (!np) { r = -1; break; }
					w->path = np; w->cap = (b + l + 1) * 2;
				}
				if (b > len) w->path[len] = '/';
				memcpy(w->path + b, p, l + 1);
				r = walk(w, b + l, (int)b, level + 1, &me);
				w->path[len] = 0;
				if (r) break;
			}
			if (cwd >= 0 && fchdir(cwd) < 0 && !r) r = -1;
		}
		if (!r && (w->flags & FTW_DEPTH)) r = call(w, FTW_DP, &st, &ftw);
	out:
		free(names);
		if (dfd >= 0) close(dfd);
		if (cwd >= 0) close(cwd);
		return r;
	}
	return call(w, type, &st, &ftw);
}

static int run(const char *path, fn_t fn, ofn_t ofn, int flags)
{
	struct w w;
	size_t l = strlen(path);
	int r, base;
	if (!*path) { errno = ENOENT; return -1; }
	w.fn = fn; w.ofn = ofn; w.flags = flags; w.dev = 0;
	w.cap = l + 256 > PATH_MAX ? l + 256 : PATH_MAX;
	if (!(w.path = malloc(w.cap))) return -1;
	memcpy(w.path, path, l + 1);
	/* base: the last component, trailing slashes aside */
	base = (int)l;
	while (base > 1 && path[base - 1] == '/') base--;
	while (base > 0 && path[base - 1] != '/') base--;
	r = walk(&w, l, base, 0, 0);
	free(w.path);
	return r;
}

int nftw(const char *path, fn_t fn, int fdlimit, int flags)
{
	(void)fdlimit;              /* at most three descriptors at a time */
	return run(path, fn, 0, flags);
}

/* ftw: links followed as by stat(); a dangling one is FTW_SL */
int __ftw(const char *path, ofn_t fn, int ndirs)
{
	if (ndirs < 1) { errno = EINVAL; return -1; }
	return run(path, 0, fn, 0);
}

extern __typeof(__ftw) ftw __attribute__((weak, alias("__ftw")));
