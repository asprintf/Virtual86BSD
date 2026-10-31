/* Differential: <search.h> - hsearch, tsearch/tfind/tdelete/twalk (the
 * in-order sequence of nodes only: tree shapes differ), lsearch/lfind,
 * insque/remque. */
#define _XOPEN_SOURCE 800
#include <search.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

static int cmpint(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

static int count;
static void walker(const void *n, VISIT v, int depth)
{
	(void)depth;
	if (v == postorder || v == leaf) {
		oi(**(int *const *)n);
		oc(' ');
		count++;
	}
}

struct el { struct el *next, *prev; int v; };

int main(void)
{
	static char keys[2000][12];
	static int vals[3000];
	ENTRY e, *r;
	void *root = 0;
	int i, k, n;
	size_t nel;
	int arr[64];

	/* hsearch */
	oi(hcreate(1000)); onl();   /* (glibc's table cannot grow: stay below nel) */
	for (i = 0; i < 2000; i++) {
		k = (int)(rng() % 1000);
		memcpy(keys[i], "key", 3);
		keys[i][3] = (char)('0' + k / 100); keys[i][4] = (char)('0' + k / 10 % 10); keys[i][5] = (char)('0' + k % 10);
		keys[i][6] = 0;
		e.key = keys[i];
		e.data = (void *)(long)i;
		r = hsearch(e, ENTER);
		oi(r ? (long)r->data : -1); oc(' ');
	}
	onl();
	for (i = 0; i < 1100; i++) {
		char q[12] = "key";
		q[3] = (char)('0' + i / 100 % 10); q[4] = (char)('0' + i / 10 % 10); q[5] = (char)('0' + i % 10); q[6] = 0;
		if (i >= 1000) q[2] = 'Y';
		e.key = q;
		errno = 0;
		r = hsearch(e, FIND);
		oi(r ? (long)r->data : -1); oc(r ? ' ' : (errno == ESRCH ? 's' : '?'));
	}
	onl();
	hdestroy();
	oi(hcreate(0)); onl();
	e.key = "x"; e.data = 0;
	oi(hsearch(e, FIND) == 0); oi(hsearch(e, ENTER) != 0); oi(hsearch(e, FIND) != 0); onl();
	hdestroy();

	/* tsearch */
	for (i = 0; i < 3000; i++)
		vals[i] = (int)(rng() % 2000);
	for (i = 0; i < 3000; i++) {
		void *p = tsearch(&vals[i], &root, cmpint);
		/* a duplicate returns the existing node */
		oi(**(int **)p == vals[i]); oi(*(int **)p == &vals[i]);
		if (i % 100 == 99) onl();
	}
	count = 0;
	twalk(root, walker); onl();
	oi(count); onl();
	for (i = 0; i < 2100; i += 7) {
		int x = i;
		void *p = tfind(&x, &root, cmpint);
		oi(p ? **(int **)p : -1); oc(' ');
	}
	onl();
	for (i = 0; i < 2000; i += 3) {
		int x = i;
		void *p = tdelete(&x, &root, cmpint);
		oc(p ? 'd' : '-');
		if (tfind(&x, &root, cmpint)) os("still there");
	}
	onl();
	count = 0;
	twalk(root, walker); onl();
	oi(count); onl();
	/* delete everything */
	for (i = 0; i < 2000; i++)
		tdelete(&i, &root, cmpint);
	oi(root == 0); onl();
	oi(tfind(&i, &root, cmpint) == 0); oi(tdelete(&i, &root, cmpint) == 0); onl();
	twalk(0, walker); onl();

	/* lsearch, lfind */
	nel = 0;
	for (i = 0; i < 200; i++) {
		k = (int)(rng() % 50);
		int *p = lsearch(&k, arr, &nel, sizeof *arr, cmpint);
		oi(p - arr); oc(' ');
	}
	onl();
	oi((long)nel); onl();
	for (i = 0; i < 60; i++) {
		int *p = lfind(&i, arr, &nel, sizeof *arr, cmpint);
		oi(p ? p - arr : -1); oc(' ');
	}
	onl();

	/* insque, remque */
	{
		struct el a[5], *p;
		for (i = 0; i < 5; i++) a[i].v = i;
		insque(&a[0], 0);
		insque(&a[1], &a[0]);
		insque(&a[2], &a[1]);
		insque(&a[3], &a[0]);   /* 0 3 1 2 */
		insque(&a[4], &a[2]);   /* 0 3 1 2 4 */
		for (p = &a[0]; p; p = p->next) oi(p->v);
		onl();
		remque(&a[1]);
		remque(&a[0]);
		remque(&a[4]);
		for (p = &a[3]; p; p = p->next) oi(p->v);
		oi(a[3].prev == 0); oi(a[2].next == 0);
		onl();
		/* a circular list */
		a[0].next = a[0].prev = &a[0];
		insque(&a[1], &a[0]);
		insque(&a[2], &a[1]);
		for (n = 0, p = a[0].next; p != &a[0]; p = p->next) oi(p->v), n++;
		remque(&a[1]);
		for (p = a[0].next; p != &a[0]; p = p->next) oi(p->v);
		onl();
	}
	oflush();
	return 0;
}
