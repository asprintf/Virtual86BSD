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
