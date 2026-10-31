/* hcreate, hsearch, hdestroy: one table per process, open addressing
 * with linear probing on a power-of-two table that doubles when it is
 * three quarters full (so ENTER fails only when memory runs out; nel is
 * the initial size estimate).  Errors as glibc: ENOMEM, ESRCH. */
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static ENTRY *tab;
static size_t mask, used;

static size_t hash(const char *s)
{
	size_t h = 5381;
	while (*s)
		h = h * 33 ^ (unsigned char)*s++;
	return h;
}

static ENTRY *slot(ENTRY *t, size_t m, const char *key)
{
	size_t i = hash(key) & m;
	while (t[i].key && strcmp(t[i].key, key))
		i = (i + 1) & m;
	return &t[i];
}

static int resize(size_t n)
{
	size_t sz = 8, i;
	ENTRY *t;
	while (sz < n + n / 3 + 1 && sz < (size_t)-1 / 4 / sizeof *t)
		sz *= 2;
	if (sz < n || !(t = calloc(sz, sizeof *t)))
		return 0;
	if (tab) {
		for (i = 0; i <= mask; i++)
			if (tab[i].key)
				*slot(t, sz - 1, tab[i].key) = tab[i];
		free(tab);
	}
	tab = t;
	mask = sz - 1;
	return 1;
}

int hcreate(size_t nel)
{
	if (tab)
		return 0;
	used = 0;
	if (!resize(nel)) {
		errno = ENOMEM;
		return 0;
	}
	return 1;
}

void hdestroy(void)
{
	free(tab);
	tab = 0;
	mask = used = 0;
}

ENTRY *hsearch(ENTRY item, ACTION action)
{
	ENTRY *e;
	if (!tab) {
		errno = action == ENTER ? ENOMEM : ESRCH;
		return 0;
	}
	e = slot(tab, mask, item.key);
	if (e->key)
		return e;
	if (action == FIND) {
		errno = ESRCH;
		return 0;
	}
	if ((used + 1) * 4 > (mask + 1) * 3) {
		if (!resize(used + 1)) {
			errno = ENOMEM;
			return 0;
		}
		e = slot(tab, mask, item.key);
	}
	*e = item;
	used++;
	return e;
}
