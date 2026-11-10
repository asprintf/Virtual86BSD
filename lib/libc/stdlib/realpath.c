/* realpath(): resolves ".", ".." and symbolic links component by
 * component (without /proc), following at most SYMLOOP_MAX links. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

char *realpath(const char *__RESTRICT path, char *__RESTRICT resolved)
{
	char rest[2 * PATH_MAX + 2], out[PATH_MAX], target[PATH_MAX];
	size_t q, rl, l;
	int links = 0, trailing;

	if (!path) { errno = EINVAL; return 0; }
	l = strlen(path);
	if (!l) { errno = ENOENT; return 0; }
	if (l >= PATH_MAX) { errno = ENAMETOOLONG; return 0; }
	trailing = path[l - 1] == '/';
	memcpy(rest, path, l + 1);
	if (*path == '/') { out[0] = '/'; out[1] = 0; q = 1; }
	else { if (!getcwd(out, sizeof out)) return 0; q = strlen(out); }

	for (;;) {
		char *s = rest, *c;
		size_t cl;
		ssize_t k;
		while (*s == '/') s++;
		if (!*s) break;
		c = s;
		cl = strcspn(c, "/");
		s = c + cl;
		/* rest := what follows this component */
		if (cl == 1 && c[0] == '.') { memmove(rest, s, strlen(s) + 1); continue; }
		if (cl == 2 && c[0] == '.' && c[1] == '.') {
			while (q > 1 && out[q - 1] != '/') q--;
			if (q > 1) q--;
			out[q] = 0;
			memmove(rest, s, strlen(s) + 1);
			continue;
		}
		if (q + 1 + cl >= sizeof out) { errno = ENAMETOOLONG; return 0; }
		if (q > 1) out[q++] = '/';
		memcpy(out + q, c, cl);
		q += cl;
		out[q] = 0;
		memmove(rest, s, strlen(s) + 1);
		k = readlink(out, target, sizeof target);
		if (k < 0) {
			if (errno != EINVAL) return 0;   /* missing, or not a directory */
			continue;                        /* not a link */
		}
		if ((size_t)k >= sizeof target) { errno = ENAMETOOLONG; return 0; }
		if (++links > SYMLOOP_MAX) { errno = ELOOP; return 0; }
		/* the link's target, then the rest */
		rl = strlen(rest);
		if ((size_t)k + 1 + rl >= sizeof rest) { errno = ENAMETOOLONG; return 0; }
		memmove(rest + k + 1, rest, rl + 1);
		memcpy(rest, target, (size_t)k);
		rest[k] = '/';
		if (target[0] == '/') { q = 1; out[1] = 0; }
		else { while (q > 1 && out[q - 1] != '/') q--; if (q > 1) q--; out[q] = 0; }
	}
	if (trailing) {
		struct stat st;
		if (stat(out, &st)) return 0;
		if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return 0; }
	}
	if (!resolved) {
		char *r = malloc(q + 1);
		return r ? memcpy(r, out, q + 1) : 0;
	}
	return memcpy(resolved, out, q + 1);
}
