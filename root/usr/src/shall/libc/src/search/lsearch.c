/* lsearch, lfind, insque, remque */
#include <search.h>
#include <string.h>

void *lfind(const void *key, const void *base, size_t *nelp, size_t width, int (*cmp)(const void *, const void *))
{
	const char *p = base;
	size_t i, n = *nelp;
	for (i = 0; i < n; i++, p += width)
		if (!cmp(key, p))
			return (void *)p;
	return 0;
}

void *lsearch(const void *key, void *base, size_t *nelp, size_t width, int (*cmp)(const void *, const void *))
{
	void *p = lfind(key, base, nelp, width, cmp);
	if (p)
		return p;
	p = (char *)base + width * *nelp;
	memcpy(p, key, width);
	++*nelp;
	return p;
}

struct q {
	struct q *next, *prev;
};

void insque(void *elem, void *pred)
{
	struct q *e = elem, *p = pred;
	if (!p) {
		e->next = e->prev = 0;
		return;
	}
	e->next = p->next;
	e->prev = p;
	p->next = e;
	if (e->next)
		e->next->prev = e;
}

void remque(void *elem)
{
	struct q *e = elem;
	if (e->next)
		e->next->prev = e->prev;
	if (e->prev)
		e->prev->next = e->next;
}
