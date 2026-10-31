#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pwf.h"

int __getgr_r(const char *name, gid_t gid, struct group *gr, char *buf, size_t size, struct group **res)
{
	FILE *f;
	char *line = 0, *p, **m = 0, **v;
	size_t n = 0, nm = 0, need, i, k, l;
	struct group t;
	int e = errno, r = 0, pr;
	*res = 0;
	if (!(f = fopen(GROUP_FILE, "re"))) {
		r = errno == ENOENT ? 0 : errno;
		errno = e;
		return r;
	}
	for (;;) {
		errno = 0;
		if (getline(&line, &n, f) < 0) { r = ferror(f) ? errno : 0; break; }
		if ((pr = __parse_gr(line, &t, &m, &nm)) == -2) { r = ENOMEM; break; }
		if (pr) continue;
		if (name ? strcmp(name, t.gr_name) : gid != t.gr_gid) continue;
		for (k = 0; t.gr_mem[k]; k++);
		/* pointer array first (aligned), then the strings */
		i = (size_t)(-(unsigned long)buf & (sizeof(char *) - 1));
		need = i + (k + 1) * sizeof(char *) + strlen(t.gr_name) + strlen(t.gr_passwd) + 2;
		for (l = 0; l < k; l++) need += strlen(t.gr_mem[l]) + 1;
		if (need > size) { r = ERANGE; break; }
		v = (char **)(void *)(buf + i);
		p = (char *)(v + k + 1);
		for (l = 0; l < k; l++) { v[l] = p; p = stpcpy(p, t.gr_mem[l]) + 1; }
		v[k] = 0;
		gr->gr_mem = v;
		gr->gr_name = p; p = stpcpy(p, t.gr_name) + 1;
		gr->gr_passwd = p; stpcpy(p, t.gr_passwd);
		gr->gr_gid = t.gr_gid;
		*res = gr;
		break;
	}
	free(m);
	free(line);
	fclose(f);
	errno = e;
	return r;
}

int getgrnam_r(const char *name, struct group *gr, char *buf, size_t size, struct group **res)
{
	return __getgr_r(name, 0, gr, buf, size, res);
}

int getgrgid_r(gid_t gid, struct group *gr, char *buf, size_t size, struct group **res)
{
	return __getgr_r(0, gid, gr, buf, size, res);
}

static struct group *getgr(const char *name, gid_t gid)
{
	static struct group gr;
	static char *buf;
	static size_t size;
	struct group *res;
	char *nb;
	int r;
	while ((r = __getgr_r(name, gid, &gr, buf, size, &res)) == ERANGE) {
		if (!(nb = realloc(buf, size ? 2 * size : 512))) return 0;
		buf = nb;
		size = size ? 2 * size : 512;
	}
	if (r) errno = r;
	return res;
}

struct group *getgrnam(const char *name) { return getgr(name, 0); }
struct group *getgrgid(gid_t gid) { return getgr(0, gid); }
