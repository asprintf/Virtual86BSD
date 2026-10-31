/* Message catalogs in the format of the GNU C library (which the gencat
 * of the POSIX utilities writes): the magic number, the size and depth of
 * a hash table, the table in little- and in big-endian words (set + 1,
 * message, offset of the text), the texts. */
#include <nl_types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <locale.h>
#include <sys/mman.h>
#include "intl_impl.h"
#include "locale_impl.h"
#include "libc.h"

#define MAGIC 0x960408deu

struct cat {
	unsigned char *map;
	size_t size;
	uint32_t hsize, depth;
	const uint32_t *tab;        /* the table in this machine's byte order */
	const char *strs;
	size_t strsize;
};

static int tryopen(const char *path, void *arg)
{
	struct cat **out = arg, *c;
	size_t size;
	unsigned char *m = __map_file(path, &size);
	uint32_t h[3];
	uint64_t ent;
	if (!m) return 0;
	if (size < 12) goto bad;
	memcpy(h, m, 12);
	if (h[0] != MAGIC || !h[1] || !h[2]) goto bad;
	ent = (uint64_t)h[1] * h[2];
	if (ent > (size - 12) / 24) goto bad;
	if (!(c = malloc(sizeof *c))) goto bad;
	c->map = m;
	c->size = size;
	c->hsize = h[1];
	c->depth = h[2];
	c->tab = (const uint32_t *)(const void *)(m + 12);   /* little-endian table first */
	c->strs = (const char *)m + 12 + 24 * ent;
	c->strsize = size - 12 - 24 * ent;
	*out = c;
	return 1;
bad:
	munmap(m, size);
	return 0;
}

nl_catd catopen(const char *name, int oflag)
{
	struct cat *c = 0;
	const char *spec, *loc;
	if (strchr(name, '/')) {
		if (!tryopen(name, &c)) { errno = ENOENT; return (nl_catd)-1; }
		return c;
	}
	if (oflag == NL_CAT_LOCALE) loc = CURRENT_LOCALE->cat[LC_MESSAGES]->name;
	else { loc = getenv("LANG"); if (!loc || !*loc) loc = "C"; }
	spec = __libc.secure ? 0 : getenv("NLSPATH");
	if (!spec || !*spec)
		spec = "/usr/share/locale/%L/%N:/usr/share/locale/%L/LC_MESSAGES/%N:"
		       "/usr/share/locale/%l/%N:/usr/share/locale/%l/LC_MESSAGES/%N";
	if (!__nlspath(spec, name, loc, tryopen, &c)) { errno = ENOENT; return (nl_catd)-1; }
	return c;
}

char *catgets(nl_catd cd, int set, int msg, const char *s)
{
	struct cat *c = cd;
	uint32_t idx, d;
	if (!c || cd == (nl_catd)-1) { errno = EBADF; return (char *)s; }
	if (set < 1 || msg < 1) { errno = ENOMSG; return (char *)s; }
	idx = (uint32_t)(((uint64_t)(unsigned)(set + 1) * (unsigned)msg) % c->hsize);
	for (d = 0; d < c->depth; d++) {
		const uint32_t *e = c->tab + 3 * ((size_t)d * c->hsize + idx);
		if (e[0] == (uint32_t)set + 1 && e[1] == (uint32_t)msg) {
			if (e[2] >= c->strsize || !memchr(c->strs + e[2], 0, c->strsize - e[2])) break;
			return (char *)c->strs + e[2];
		}
	}
	errno = ENOMSG;
	return (char *)s;
}

int catclose(nl_catd cd)
{
	struct cat *c = cd;
	if (!c || cd == (nl_catd)-1) { errno = EBADF; return -1; }
	munmap(c->map, c->size);
	free(c);
	return 0;
}
