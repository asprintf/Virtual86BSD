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

/* tsearch, tfind, tdelete, twalk: an AVL tree.  A node's first member
 * is the key pointer, as the interface requires (the application reads
 * *(void **)node).  Insertion and deletion keep the path from the root
 * in an array (an AVL tree of 2^64 nodes is less than 93 deep). */
#include <search.h>
#include <stdlib.h>

struct node {
	const void *key;
	struct node *a[2];
	int h;
};

#define MAXH 100

static int height(struct node *n) { return n ? n->h : 0; }

static int rot(struct node **p, struct node *x, int dir)
{
	/* rotate the subtree x (at *p) in direction dir (0 left, 1 right) */
	struct node *y = x->a[!dir], *z = y->a[dir];
	int hx = x->h, hz = height(z);
	if (hz > height(y->a[!dir])) {
		/* double rotation: z becomes the root */
		x->a[!dir] = z->a[dir];
		y->a[dir] = z->a[!dir];
		z->a[dir] = x;
		z->a[!dir] = y;
		x->h = y->h = hz;
		z->h = hz + 1;
		*p = z;
		return z->h - hx;
	}
	x->a[!dir] = z;
	y->a[dir] = x;
	x->h = hz + 1;
	y->h = hz + 2;
	*p = y;
	return y->h - hx;
}

/* restore the balance at *p; returns how much its height changed */
static int balance(struct node **p)
{
	struct node *n = *p;
	int h0 = height(n->a[0]), h1 = height(n->a[1]), old = n->h;
	if (h0 - h1 + 1u < 3u) {
		n->h = (h0 > h1 ? h0 : h1) + 1;
		return n->h - old;
	}
	return rot(p, n, h0 > h1);   /* rotate away from the heavy side */
}

posix_tnode *tsearch(const void *key, posix_tnode **rootp, int (*cmp)(const void *, const void *))
{
	struct node **a[MAXH], *n, *r;
	int i = 0, c;
	if (!rootp)
		return 0;
	a[i++] = (struct node **)rootp;
	for (n = *(struct node **)rootp; n; n = n->a[c > 0]) {
		if (!(c = cmp(key, n->key)))
			return n;
		a[i++] = &n->a[c > 0];
	}
	if (!(r = malloc(sizeof *r)))
		return 0;
	r->key = key;
	r->a[0] = r->a[1] = 0;
	r->h = 1;
	*a[--i] = r;
	while (i && balance(a[--i]))
		;
	return r;
}

posix_tnode *tfind(const void *key, posix_tnode *const *rootp, int (*cmp)(const void *, const void *))
{
	struct node *n;
	int c;
	if (!rootp)
		return 0;
	for (n = *(struct node *const *)rootp; n; n = n->a[c > 0])
		if (!(c = cmp(key, n->key)))
			break;
	return n;
}

void *tdelete(const void *__RESTRICT key, posix_tnode **__RESTRICT rootp, int (*cmp)(const void *, const void *))
{
	struct node **a[MAXH], *n, *child, *parent = 0;
	int i = 0, c;
	if (!rootp)
		return 0;
	a[i++] = (struct node **)rootp;
	for (n = *(struct node **)rootp;; n = n->a[c > 0]) {
		if (!n)
			return 0;
		if (!(c = cmp(key, n->key)))
			break;
		parent = n;
		a[i++] = &n->a[c > 0];
	}
	if (n->a[0]) {
		/* replace n's key by its in-order predecessor's, remove that */
		struct node *del = n;
		a[i++] = &n->a[0];
		for (n = n->a[0]; n->a[1]; n = n->a[1])
			a[i++] = &n->a[1];
		del->key = n->key;
		child = n->a[0];
	} else
		child = n->a[1];
	free(n);
	*a[--i] = child;
	while (i > 0 && balance(a[--i]))
		;
	return parent ? (void *)parent : (void *)rootp;
}

static void walk(const struct node *n, void (*act)(const posix_tnode *, VISIT, int), int d)
{
	if (!n)
		return;
	if (!n->a[0] && !n->a[1]) {
		act(n, leaf, d);
		return;
	}
	act(n, preorder, d);
	walk(n->a[0], act, d + 1);
	act(n, postorder, d);
	walk(n->a[1], act, d + 1);
	act(n, endorder, d);
}

void twalk(const posix_tnode *root, void (*act)(const posix_tnode *, VISIT, int))
{
	if (act)
		walk(root, act, 0);
}
