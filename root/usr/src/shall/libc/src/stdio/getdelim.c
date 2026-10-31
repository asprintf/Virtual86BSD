#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "stdio_impl.h"

ssize_t getdelim(char **__RESTRICT s, size_t *__RESTRICT n, int delim, FILE *__RESTRICT f)
{
	size_t i = 0;
	int c;
	if (!s || !n) { errno = EINVAL; return -1; }
	if (!*s) *n = 0;
	FLOCK(f);
	for (;;) {
		if (i + 1 >= *n) {
			size_t m = *n < 64 ? 128 : 2 * *n;
			char *t;
			if (m > (size_t)SIZE_MAX / 2 || m < *n) { errno = EOVERFLOW; goto fail; }
			if (!(t = realloc(*s, m))) { errno = ENOMEM; goto fail; }
			*s = t;
			*n = m;
		}
		/* copy straight from the buffer up to the delimiter */
		if (f->rpos != f->rend) {
			unsigned char *z = memchr(f->rpos, delim, (size_t)(f->rend - f->rpos));
			size_t k = z ? (size_t)(z - f->rpos) + 1 : (size_t)(f->rend - f->rpos);
			int done = z != 0;
			if (k > *n - i - 1) { k = *n - i - 1; done = 0; }
			memcpy(*s + i, f->rpos, k);
			f->rpos += k;
			i += k;
			if (done) break;
			continue;
		}
		if ((c = __uflow(f)) == EOF) {
			if (!i || !(f->flags & F_EOF)) goto fail_keep;
			break;
		}
		(*s)[i++] = (char)c;
		if (c == delim) break;
	}
	(*s)[i] = 0;
	FUNLOCK(f);
	return (ssize_t)i;
fail:
	f->flags |= F_ERR;
fail_keep:
	if (*s && *n) (*s)[i < *n ? i : 0] = 0;
	FUNLOCK(f);
	return -1;
}

ssize_t getline(char **__RESTRICT s, size_t *__RESTRICT n, FILE *__RESTRICT f)
{
	return getdelim(s, n, '\n', f);
}
