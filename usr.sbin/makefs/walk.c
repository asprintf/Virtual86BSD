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
/*
 * makefs: read the directory tree.  Each directory is a list of fsnode (its
 * entries, by name), a hard-linked file is one fsinode under several names.
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "makefs.h"

/* the files with more than one name seen so far: by device and inode number */
struct seen {
	dev_t		 dev;
	ino_t		 ino;
	fsinode		*inode;
	struct seen	*next;
};

static struct seen *seen;

static fsinode *
inode_of(const struct stat *st)
{
	struct seen *s;
	fsinode *ip;

	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1)
		for (s = seen; s != NULL; s = s->next)
			if (s->dev == st->st_dev && s->ino == st->st_ino) {
				s->inode->nlink++;
				return s->inode;
			}
	ip = ecalloc(1, sizeof(*ip));
	ip->st = *st;
	ip->nlink = 1;
	if (rootowner)
		ip->st.st_uid = ip->st.st_gid = 0;
	if (stamped) {
		ip->st.st_atim.tv_sec = ip->st.st_mtim.tv_sec = ip->st.st_ctim.tv_sec = stamp;
		ip->st.st_atim.tv_nsec = ip->st.st_mtim.tv_nsec = ip->st.st_ctim.tv_nsec = 0;
	}
	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
		s = ecalloc(1, sizeof(*s));
		s->dev = st->st_dev;
		s->ino = st->st_ino;
		s->inode = ip;
		s->next = seen;
		seen = s;
	}
	return ip;
}

static int
byname(const void *a, const void *b)
{
	return strcmp((*(fsnode *const *)a)->name, (*(fsnode *const *)b)->name);
}

/* the entries of the directory at path, under parent, sorted by name */
static fsnode *
read_dir(const char *path, fsnode *parent)
{
	DIR *dp;
	struct dirent *de;
	fsnode **v = NULL, *head = NULL, **tail = &head;
	size_t n = 0, cap = 0, i;
	char *sub;

	if ((dp = opendir(path)) == NULL)
		err(EXIT_FAILURE, "Can't open directory `%s'", path);
	while ((de = readdir(dp)) != NULL) {
		fsnode *np;
		struct stat st;
		size_t len;

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		len = strlen(path) + 1 + strlen(de->d_name) + 1;
		sub = emalloc(len);
		snprintf(sub, len, "%s/%s", path, de->d_name);
		if (lstat(sub, &st) == -1)
			err(EXIT_FAILURE, "Can't stat `%s'", sub);
		np = ecalloc(1, sizeof(*np));
		np->name = estrdup(de->d_name);
		np->path = sub;
		np->parent = parent;
		np->type = st.st_mode & S_IFMT;
		np->inode = inode_of(&st);
		if (S_ISLNK(st.st_mode)) {
			char target[PATH_MAX + 1];
			ssize_t l = readlink(sub, target, sizeof(target) - 1);

			if (l == -1)
				err(EXIT_FAILURE, "Can't read the link `%s'", sub);
			target[l] = '\0';
			np->symlink = estrdup(target);
		}
		if (n == cap) {
			cap = cap ? cap * 2 : 64;
			v = erealloc(v, cap * sizeof(*v));
		}
		v[n++] = np;
	}
	closedir(dp);
	if (n > 1)
		qsort(v, n, sizeof(*v), byname);
	for (i = 0; i < n; i++) {
		*tail = v[i];
		tail = &v[i]->next;
	}
	for (i = 0; i < n; i++)
		if (v[i]->type == S_IFDIR)
			v[i]->child = read_dir(v[i]->path, v[i]);
	free(v);
	return head;
}

/* the tree under the directory dir: the node of dir itself, with its entries below */
fsnode *
walk_dir(const char *dir)
{
	struct stat st;
	fsnode *np;

	if (stat(dir, &st) == -1)
		err(EXIT_FAILURE, "Can't stat `%s'", dir);
	if (!S_ISDIR(st.st_mode))
		errx(EXIT_FAILURE, "%s: not a directory", dir);
	np = ecalloc(1, sizeof(*np));
	np->name = estrdup(".");
	np->path = estrdup(dir);
	np->type = S_IFDIR;
	np->inode = inode_of(&st);
	np->child = read_dir(dir, np);
	return np;
}

void
free_fsnodes(fsnode *node)
{
	fsnode *next;

	for (; node != NULL; node = next) {
		next = node->next;
		free_fsnodes(node->child);
		free(node->name);
		free(node->path);
		free(node->symlink);
		free(node);
	}
}
