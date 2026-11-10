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

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <wchar.h>
#include "regex_impl.h"
#include "locale_impl.h"
#include "mb_impl.h"

/* ---- memory: nodes and their child arrays live in a chain of blocks ---- */

struct blk { struct blk *next; size_t used, cap; };

static void *arena_alloc(void **arena, size_t n)
{
	struct blk *b = *arena;
	n = (n + 15) & ~(size_t)15;
	if (!b || b->cap - b->used < n) {
		size_t cap = n > 16384 ? n : 16384;
		struct blk *nb = malloc(sizeof(struct blk) + 16 + cap);
		if (!nb) return 0;
		nb->next = b; nb->used = 0; nb->cap = cap;
		*arena = b = nb;
	}
	b->used += n;
	return (char *)(b + 1) + 16 + b->used - n;
}

static void arena_free(void *arena)
{
	struct blk *b = arena, *n;
	for (; b; b = n) { n = b->next; free(b); }
}

/* ---- parser ---- */

#define MAXNODES 200000

typedef struct {
	const unsigned char *s;
	int flags, ere, err;
	struct re_prog *g;
	void *pa;                   /* parse-tree arena */
	size_t ngroups;
	unsigned char closed[10];   /* groups 1-9 closed, for \digit */
	long nodes;                 /* nodes made by expansion */
	int u8;                     /* the locale's codeset is UTF-8 */
} P;

static struct re_node *mk(P *p, void **arena, int type, int nkid)
{
	struct re_node *n = arena_alloc(arena, sizeof *n + (size_t)nkid * sizeof(struct re_node *));
	if (!n) { p->err = REG_ESPACE; return 0; }
	memset(n, 0, sizeof *n);
	n->type = (unsigned char)type;
	n->nkid = nkid;
	n->kid = (struct re_node **)(n + 1);
	n->max = -1;
	return n;
}

static int newset(P *p)
{
	struct re_prog *g = p->g;
	unsigned char (*ns)[32];
	if (!(g->nset & 15)) {
		if (!(ns = realloc(g->sets, (size_t)(g->nset + 16) * sizeof *ns))) { p->err = REG_ESPACE; return -1; }
		g->sets = ns;
	}
	memset(g->sets[g->nset], 0, 32);
	return g->nset++;
}

static void setbit(unsigned char *m, int c) { m[c >> 3] |= (unsigned char)(1 << (c & 7)); }

static struct re_node *lit(P *p, int c)
{
	struct re_node *n;
	int k;
	if ((p->flags & REG_ICASE) && tolower(c) != toupper(c)) {
		if ((k = newset(p)) < 0) return 0;
		setbit(p->g->sets[k], tolower(c));
		setbit(p->g->sets[k], toupper(c));
		if (!(n = mk(p, &p->pa, N_SET, 0))) return 0;
		n->val = k;
		return n;
	}
	if (!(n = mk(p, &p->pa, N_CHAR, 0))) return 0;
	n->val = c;
	return n;
}

static struct re_node *anychar(P *p)
{
	struct re_node *n;
	int k = newset(p), c;
	if (k < 0) return 0;
	for (c = 0; c < 256; c++)
		if (c != '\n' || !(p->flags & REG_NEWLINE)) setbit(p->g->sets[k], c);
	if (!(n = mk(p, &p->pa, N_SET, 0))) return 0;
	n->val = k;
	return n;
}

static const struct { const char *name; int (*f)(int); } classes[] = {
	{ "alnum", isalnum }, { "alpha", isalpha }, { "blank", isblank }, { "cntrl", iscntrl },
	{ "digit", isdigit }, { "graph", isgraph }, { "lower", islower }, { "print", isprint },
	{ "punct", ispunct }, { "space", isspace }, { "upper", isupper }, { "xdigit", isxdigit },
};

/* ---- UTF-8 locales: characters as byte sequences ----
 *
 * The matcher works on bytes. In a UTF-8 locale a character of the
 * pattern becomes the sequence of its bytes, and a set of characters
 * ('.', a bracket expression) becomes an automaton over bytes: the code
 * point ranges are split into ranges of UTF-8 byte sequences, which are
 * merged into a tree on their leading byte ranges. */

typedef struct { uint32_t (*r)[2]; size_t n, cap; } WS;

static int ws_add(P *p, WS *w, uint32_t lo, uint32_t hi)
{
	if (lo > hi) return 0;
	if (w->n == w->cap) {
		uint32_t (*nr)[2];
		w->cap = w->cap ? 2 * w->cap : 32;
		if (!(nr = realloc(w->r, w->cap * sizeof *nr))) { p->err = REG_ESPACE; return -1; }
		w->r = nr;
	}
	w->r[w->n][0] = lo;
	w->r[w->n][1] = hi;
	w->n++;
	return 0;
}

struct wsadd { P *p; WS *w; };

static int ws_add_cb(void *a, uint32_t lo, uint32_t hi)
{
	struct wsadd *x = a;
	return ws_add(x->p, x->w, lo, hi) ? 1 : 0;
}

static int ws_cmp(const void *a, const void *b)
{
	uint32_t x = ((const uint32_t *)a)[0], y = ((const uint32_t *)b)[0];
	return x < y ? -1 : x > y;
}

/* sorted, merged, without the surrogates */
static void ws_norm(P *p, WS *w)
{
	size_t i, k = 0;
	if (!w->n) return;
	qsort(w->r, w->n, sizeof *w->r, ws_cmp);
	for (i = 1; i < w->n; i++) {
		if (w->r[i][0] <= w->r[k][1] + 1) { if (w->r[i][1] > w->r[k][1]) w->r[k][1] = w->r[i][1]; }
		else { k++; w->r[k][0] = w->r[i][0]; w->r[k][1] = w->r[i][1]; }
	}
	w->n = k + 1;
	/* cut out D800-DFFF */
	for (i = 0; i < w->n; i++) {
		uint32_t lo = w->r[i][0], hi = w->r[i][1];
		if (hi < 0xd800 || lo > 0xdfff) continue;
		if (lo < 0xd800 && hi > 0xdfff) {
			w->r[i][1] = 0xd7ff;
			if (ws_add(p, w, 0xe000, hi)) return;
			ws_norm(p, w);
			return;
		}
		if (lo < 0xd800) w->r[i][1] = 0xd7ff;
		else if (hi > 0xdfff) w->r[i][0] = 0xe000;
		else { w->r[i][0] = 1; w->r[i][1] = 0; }   /* empty: dropped below */
	}
	for (i = k = 0; i < w->n; i++) if (w->r[i][0] <= w->r[i][1]) { w->r[k][0] = w->r[i][0]; w->r[k][1] = w->r[i][1]; k++; }
	w->n = k;
}

static int ws_invert(P *p, WS *w)
{
	WS o = { 0 };
	uint32_t next = 0;
	size_t i;
	ws_norm(p, w);
	for (i = 0; i < w->n; i++) {
		if (w->r[i][0] > next && ws_add(p, &o, next, w->r[i][0] - 1)) { free(o.r); return -1; }
		next = w->r[i][1] + 1;
	}
	if (next <= 0x10ffff && ws_add(p, &o, next, 0x10ffff)) { free(o.r); return -1; }
	free(w->r);
	*w = o;
	ws_norm(p, w);
	return 0;
}

struct u8seq { unsigned char lo[4], hi[4]; int n; };
typedef struct { struct u8seq *v; size_t n, cap; } SEQS;

static int u8enc(uint32_t c, unsigned char *b)
{
	if (c < 0x80) { b[0] = (unsigned char)c; return 1; }
	if (c < 0x800) { b[0] = (unsigned char)(0xc0 | c >> 6); b[1] = (unsigned char)(0x80 | (c & 63)); return 2; }
	if (c < 0x10000) {
		b[0] = (unsigned char)(0xe0 | c >> 12); b[1] = (unsigned char)(0x80 | (c >> 6 & 63));
		b[2] = (unsigned char)(0x80 | (c & 63));
		return 3;
	}
	b[0] = (unsigned char)(0xf0 | c >> 18); b[1] = (unsigned char)(0x80 | (c >> 12 & 63));
	b[2] = (unsigned char)(0x80 | (c >> 6 & 63)); b[3] = (unsigned char)(0x80 | (c & 63));
	return 4;
}

static int u8split(P *p, SEQS *q, uint32_t lo, uint32_t hi)
{
	static const uint32_t maxs[3] = { 0x7f, 0x7ff, 0xffff };
	unsigned char a[4], b[4];
	int i, n;
	if (lo > hi) return 0;
	for (i = 0; i < 3; i++)
		if (lo <= maxs[i] && hi > maxs[i])
			return u8split(p, q, lo, maxs[i]) || u8split(p, q, maxs[i] + 1, hi);
	n = u8enc(lo, a);
	for (i = 1; i < n; i++) {
		uint32_t m = (1u << (6 * i)) - 1;
		if ((lo & ~m) != (hi & ~m)) {
			if (lo & m) return u8split(p, q, lo, lo | m) || u8split(p, q, (lo | m) + 1, hi);
			if ((hi & m) != m) return u8split(p, q, lo, (hi & ~m) - 1) || u8split(p, q, hi & ~m, hi);
		}
	}
	u8enc(hi, b);
	if (q->n == q->cap) {
		struct u8seq *nv;
		q->cap = q->cap ? 2 * q->cap : 64;
		if (!(nv = realloc(q->v, q->cap * sizeof *nv))) { p->err = REG_ESPACE; return -1; }
		q->v = nv;
	}
	for (i = 0; i < n; i++) { q->v[q->n].lo[i] = a[i]; q->v[q->n].hi[i] = b[i]; }
	q->v[q->n].n = n;
	q->n++;
	return 0;
}

static struct re_node *byteset(P *p, int lo, int hi)
{
	struct re_node *n;
	int k = newset(p), c;
	if (k < 0) return 0;
	for (c = lo; c <= hi; c++) setbit(p->g->sets[k], c);
	if (!(n = mk(p, &p->pa, N_SET, 0))) return 0;
	n->val = k;
	return n;
}

/* the tree of v[a..b), all of the same length, at byte d */
static struct re_node *u8tree(P *p, const struct u8seq *v, size_t a, size_t b, int d)
{
	struct re_node *alts[256], *n, *c;
	size_t i, j, na = 0;
	for (i = a; i < b; i = j) {
		for (j = i + 1; j < b && v[j].lo[d] == v[i].lo[d] && v[j].hi[d] == v[i].hi[d]; j++);
		if (!(c = byteset(p, v[i].lo[d], v[i].hi[d]))) return 0;
		if (d + 1 < v[i].n) {
			struct re_node *t = u8tree(p, v, i, j, d + 1), *cc;
			if (!t || !(cc = mk(p, &p->pa, N_CAT, 2))) return 0;
			cc->kid[0] = c; cc->kid[1] = t;
			c = cc;
		}
		if (na == 256) { p->err = REG_ESPACE; return 0; }
		alts[na++] = c;
	}
	if (na == 1) return alts[0];
	if (!(n = mk(p, &p->pa, N_ALT, (int)na))) return 0;
	for (i = 0; i < na; i++) n->kid[i] = alts[i];
	return n;
}

/* the node matching one character of the set */
static struct re_node *u8node(P *p, WS *w)
{
	SEQS q = { 0 };
	struct re_node *alts[8], *n;
	unsigned char asc[32];
	size_t i, j;
	int na = 0, k, any = 0;
	ws_norm(p, w);
	memset(asc, 0, sizeof asc);
	for (i = 0; i < w->n; i++) {
		uint32_t lo = w->r[i][0], hi = w->r[i][1];
		for (; lo <= hi && lo < 0x80; lo++) { setbit(asc, (int)lo); any = 1; }
		if (lo <= hi && u8split(p, &q, lo, hi)) goto fail;
	}
	if (any) {
		if ((k = newset(p)) < 0 || !(n = mk(p, &p->pa, N_SET, 0))) goto fail;
		memcpy(p->g->sets[k], asc, 32);
		n->val = k;
		alts[na++] = n;
	}
	/* the sequences of each length (2, 3, 4) form a tree */
	for (i = 0; i < q.n; i = j) {
		for (j = i + 1; j < q.n && q.v[j].n == q.v[i].n; j++);
		if (!(n = u8tree(p, q.v, i, j, 0))) goto fail;
		alts[na++] = n;
	}
	free(q.v);
	if (!na) return byteset(p, 1, 0);   /* nothing */
	if (na == 1) return alts[0];
	if (!(n = mk(p, &p->pa, N_ALT, na))) return 0;
	for (k = 0; k < na; k++) n->kid[k] = alts[k];
	return n;
fail:
	free(q.v);
	return 0;
}

/* case variants of the members (for REG_ICASE); large sets are left as
 * they are */
static int ws_fold(P *p, WS *w)
{
	size_t i, n = w->n, total = 0;
	const struct __lcat *ct = CURRENT_CTYPE;
	for (i = 0; i < n; i++) total += w->r[i][1] - w->r[i][0] + 1;
	if (total > 0x20000) return 0;
	for (i = 0; i < n; i++) {
		uint32_t c;
		for (c = w->r[i][0]; c <= w->r[i][1]; c++) {
			uint32_t u = __towctrans_l(c, 1, ct), l = __towctrans_l(c, 2, ct);
			if (u != c && ws_add(p, w, u, u)) return -1;
			if (l != c && ws_add(p, w, l, l)) return -1;
		}
	}
	return 0;
}

/* a multibyte character of the pattern at p->s (advanced past it) */
static struct re_node *u8char(P *p)
{
	wchar_t wc;
	mbstate_t st = { { 0, 0 } };
	size_t l = __mbrtowc_l(&wc, (const char *)p->s, 4, &st, &__c_utf8_ctype), i;
	struct re_node *n;
	if (l == (size_t)-1 || l == (size_t)-2 || l < 2) {
		/* not a character: the byte itself */
		return lit(p, *p->s++);
	}
	if (p->flags & REG_ICASE) {
		WS w = { 0 };
		if (ws_add(p, &w, (uint32_t)wc, (uint32_t)wc) || ws_fold(p, &w)) { free(w.r); return 0; }
		p->s += l;
		n = u8node(p, &w);
		free(w.r);
		return n;
	}
	if (!(n = mk(p, &p->pa, N_CAT, (int)l))) return 0;
	for (i = 0; i < l; i++) {
		struct re_node *c = mk(p, &p->pa, N_CHAR, 0);
		if (!c) return 0;
		c->val = p->s[i];
		n->kid[i] = c;
	}
	p->s += l;
	return n;
}

static struct re_node *u8any(P *p)
{
	WS w = { 0 };
	struct re_node *n;
	if (p->flags & REG_NEWLINE) {
		if (ws_add(p, &w, 0, '\n' - 1) || ws_add(p, &w, '\n' + 1, 0x10ffff)) { free(w.r); return 0; }
	} else if (ws_add(p, &w, 0, 0x10ffff)) { free(w.r); return 0; }
	n = u8node(p, &w);
	free(w.r);
	return n;
}

/* ---- bracket expressions ----
 *
 * A bracket expression matches one character of a set, or one of the
 * locale's multi-character collating elements ("ch"): those named by
 * [.ch.] and those of an equivalence class [=x=] (the same first-level
 * weights; glibc's regex never matches these).  Range expressions and
 * non-matching lists match single characters only: POSIX leaves both
 * open, and glibc's locales define hundreds of elements (decomposed
 * letters), which "[^a]" or "[a-z]" would otherwise match as well
 * (glibc's regex does so, by rules of its own).  An element becomes an
 * alternative to the set: "[[.ch.]a]" is "(ch|[a])" as far as
 * matching goes, so the longest match may take either. */

typedef struct {
	const struct __lcat *co;
	int wide;                   /* units: code points (UTF-8 patterns) or bytes */
	size_t n;                   /* the multi-character elements of the locale */
	unsigned char *in;          /* those the bracket expression matches */
} ME;

static void me_init(ME *m, int wide)
{
	m->co = CURRENT_LOCALE->cat[LC_COLLATE];
	m->wide = wide;
	m->n = 0;
	m->in = 0;
}

/* the elements equal to x[0..n) (how 0), of its equivalence class (1) */
static int me_mark(P *p, ME *m, int how, const uint32_t *x, size_t n)
{
	struct __coll_me e;
	uint32_t u[256];
	size_t pos = 0, i, nu;
	if (!m->in) {
		/* the table of elements, made when first needed */
		while (__coll_me_next(m->co, &pos, &e)) m->n++;
		if (!m->n) return 0;
		if (!(m->in = calloc(m->n, 1))) { p->err = REG_ESPACE; return -1; }
		pos = 0;
	}
	for (i = 0; __coll_me_next(m->co, &pos, &e); i++) {
		nu = __coll_me_units(&e, m->wide, u);
		if (how ? __coll_equivs(m->co, m->wide, x, n, u, nu)
		        : nu == n && !memcmp(u, x, n * sizeof *x))
			m->in[i] = 1;
	}
	return 0;
}

/* one code point of a UTF-8 pattern as a node (with its case variants) */
static struct re_node *u8cp(P *p, uint32_t c)
{
	WS w = { 0 };
	struct re_node *n = 0;
	if (!ws_add(p, &w, c, c) && (!(p->flags & REG_ICASE) || !ws_fold(p, &w)))
		n = u8node(p, &w);
	free(w.r);
	return n;
}

/* the bracket expression: set, or (set | element | ...) */
static struct re_node *me_finish(P *p, ME *m, int neg, struct re_node *set)
{
	struct __coll_me e;
	struct re_node *n = 0, *s;
	uint32_t u[256];
	size_t pos = 0, i, j, nu, k = 0;
	if (!m->in || neg) goto out;
	for (i = 0; i < m->n; i++)
		k += m->in[i];
	if (!set || !k) goto out;
	if (!(n = mk(p, &p->pa, N_ALT, (int)k + 1))) goto out;
	n->kid[0] = set;
	k = 1;
	for (i = 0; __coll_me_next(m->co, &pos, &e); i++) {
		if (!m->in[i]) continue;
		nu = __coll_me_units(&e, m->wide, u);
		if (!(s = mk(p, &p->pa, N_CAT, (int)nu))) { n = 0; goto out; }
		for (j = 0; j < nu; j++) {
			if (!(s->kid[j] = m->wide ? u8cp(p, u[j]) : lit(p, (int)u[j]))) { n = 0; goto out; }
		}
		if (!(n->kid[k++] = s)) { n = 0; goto out; }
	}
out:
	free(m->in);
	m->in = 0;
	return k ? n : set;
}

/* "[.name.]" or "[=name=]" at p->s: its units (code points of a UTF-8
 * pattern, else bytes) into u[256] */
static int collname(P *p, int wide, uint32_t *u, size_t *n)
{
	const unsigned char *s = p->s, *e = s + 2;
	int d = s[1];
	while (*e && !(e[0] == d && e[1] == ']')) e++;
	if (!*e) { p->err = REG_EBRACK; return -1; }
	*n = 0;
	for (s += 2; s < e; ) {
		if (*n == 255) { p->err = REG_ECOLLATE; return -1; }
		if (wide) {
			wchar_t wc;
			mbstate_t st = { { 0, 0 } };
			size_t l = __mbrtowc_l(&wc, (const char *)s, (size_t)(e - s), &st, &__c_utf8_ctype);
			if (l == (size_t)-1 || l == (size_t)-2 || !l) { p->err = REG_ECOLLATE; return -1; }
			u[(*n)++] = (uint32_t)wc;
			s += l;
		} else
			u[(*n)++] = *s++;
	}
	if (!*n) { p->err = REG_ECOLLATE; return -1; }
	p->s = e + 2;
	return 0;
}

/* a range end point or a single item: a character, or a collating symbol
 * (one character, or a multi-character collating element of the locale) */
static int brelem(P *p, ME *m, uint32_t *u, size_t *n)
{
	const unsigned char *s = p->s;
	if (s[0] == '[' && s[1] == '.') {
		if (collname(p, m->wide, u, n)) return -1;
		if (*n > 1 && !__coll_valids(m->co, m->wide, u, *n)) { p->err = REG_ECOLLATE; return -1; }
		return 0;
	}
	*n = 1;
	if (m->wide) {
		wchar_t wc;
		mbstate_t st = { { 0, 0 } };
		size_t l = __mbrtowc_l(&wc, (const char *)s, 4, &st, &__c_utf8_ctype);
		if (l == (size_t)-1 || l == (size_t)-2 || !l) { *u = *s; p->s = s + 1; return 0; }
		*u = (uint32_t)wc;
		p->s = s + l;
	} else {
		*u = *s;
		p->s = s + 1;
	}
	return 0;
}

/* the collation sequence of a byte (glibc: of its wide character, as for
 * the multi-character elements, which have no other) */
static uint32_t bseq(const struct __lcat *co, uint32_t k)
{
	wchar_t wc;
	mbstate_t st = { { 0, 0 } };
	unsigned char b = (unsigned char)k;
	if (__mbrtowc_l(&wc, (const char *)&b, 1, &st, CURRENT_CTYPE) > 1) return ~0u;
	return __coll_seq(co, 1, (uint32_t)wc);
}

static uint32_t nseq(const struct __lcat *co, const uint32_t *u, size_t n)
{
	return n == 1 ? bseq(co, *u) : __coll_seqs(co, 0, u, n);
}

/* a bracket expression in a locale of single bytes */
static struct re_node *bracket(P *p)
{
	unsigned char m[32];
	int neg = 0, first = 1, c, k, i;
	uint32_t a[256], b[256];
	size_t na, nb;
	ME me;
	struct re_node *n;
	const struct __lcat *co = CURRENT_LOCALE->cat[LC_COLLATE];
	memset(m, 0, sizeof m);
	me_init(&me, 0);
	if (*p->s == '^') { neg = 1; p->s++; }
	for (;;) {
		const unsigned char *s = p->s;
		if (!*s) { p->err = REG_EBRACK; goto fail; }
		if (*s == ']' && !first) { p->s++; break; }
		first = 0;
		if (s[0] == '[' && s[1] == ':') {
			const unsigned char *e = s + 2;
			size_t l;
			while (*e && !(e[0] == ':' && e[1] == ']')) e++;
			if (!*e) { p->err = REG_EBRACK; goto fail; }
			l = (size_t)(e - (s + 2));
			for (i = 0; i < (int)(sizeof classes / sizeof *classes); i++)
				if (strlen(classes[i].name) == l && !memcmp(classes[i].name, s + 2, l)) break;
			if (i == (int)(sizeof classes / sizeof *classes)) { p->err = REG_ECTYPE; goto fail; }
			for (c = 0; c < 256; c++) if (classes[i].f(c)) setbit(m, c);
			if ((p->flags & REG_ICASE) && (classes[i].f == isupper || classes[i].f == islower))
				for (c = 0; c < 256; c++) if (isalpha(c)) setbit(m, c);
			p->s = e + 2;
			if (p->s[0] == '-' && p->s[1] != ']' && p->s[1]) { p->err = REG_ERANGE; goto fail; }
			continue;
		}
		if (s[0] == '[' && s[1] == '=') {
			/* the characters and elements of its equivalence class (LC_COLLATE) */
			if (collname(p, 0, a, &na)) goto fail;
			if (!__coll_valids(co, 0, a, na)) { p->err = REG_ECOLLATE; goto fail; }
			for (k = 0; k < 256; k++) {
				uint32_t x = (uint32_t)k;
				if (__coll_equivs(co, 0, a, na, &x, 1)) setbit(m, k);
			}
			if (me_mark(p, &me, 1, a, na)) goto fail;
			continue;
		}
		if (brelem(p, &me, a, &na)) goto fail;
		if (p->s[0] == '-' && p->s[1] && p->s[1] != ']') {
			p->s++;
			if (p->s[0] == '[' && (p->s[1] == '=' || p->s[1] == ':')) { p->err = REG_ERANGE; goto fail; }
			if (brelem(p, &me, b, &nb)) goto fail;
			if (co->nrules && co->collseqwc) {
				/* in a locale with collation rules, by the collation
				 * sequence (as glibc); else by the bytes */
				uint32_t sc = nseq(co, a, na), sd = nseq(co, b, nb), sk;
				if (sc == ~0u || sd == ~0u) { p->err = REG_ECOLLATE; goto fail; }
				if (sc > sd) { p->err = REG_ERANGE; goto fail; }
				for (k = 0; k < 256; k++)
					if ((sk = bseq(co, (uint32_t)k)) >= sc && sk <= sd) setbit(m, k);
			} else {
				if (b[0] < a[0]) { p->err = REG_ERANGE; goto fail; }
				for (k = (int)a[0]; k <= (int)b[0]; k++) setbit(m, k);
			}
		} else if (na == 1)
			setbit(m, (int)a[0]);
		else
			if (me_mark(p, &me, 0, a, na)) goto fail;
	}
	if (p->flags & REG_ICASE)
		for (c = 0; c < 256; c++)
			if (m[c >> 3] & (1 << (c & 7))) { setbit(m, tolower(c)); setbit(m, toupper(c)); }
	if (neg) {
		for (i = 0; i < 32; i++) m[i] = (unsigned char)~m[i];
		if (p->flags & REG_NEWLINE) m['\n' >> 3] &= (unsigned char)~(1 << ('\n' & 7));
	}
	if ((k = newset(p)) < 0) goto fail;
	memcpy(p->g->sets[k], m, 32);
	if (!(n = mk(p, &p->pa, N_SET, 0))) goto fail;
	n->val = k;
	return me_finish(p, &me, neg, n);
fail:
	free(me.in);
	return 0;
}


/* a bracket expression in a UTF-8 pattern */
static struct re_node *u8bracket(P *p)
{
	const struct __lcat *ct = CURRENT_CTYPE, *co = CURRENT_LOCALE->cat[LC_COLLATE];
	WS w = { 0 };
	int neg = 0, first = 1;
	uint32_t a[256], b[256];
	size_t na, nb;
	ME me;
	struct re_node *n;
	me_init(&me, 1);
	if (*p->s == '^') { neg = 1; p->s++; }
	for (;;) {
		const unsigned char *s = p->s;
		if (!*s) { p->err = REG_EBRACK; goto fail; }
		if (*s == ']' && !first) { p->s++; break; }
		first = 0;
		if (s[0] == '[' && s[1] == ':') {
			const unsigned char *e = s + 2;
			size_t l;
			unsigned long t = 0;
			char name[32];
			while (*e && !(e[0] == ':' && e[1] == ']')) e++;
			if (!*e) { p->err = REG_EBRACK; goto fail; }
			l = (size_t)(e - (s + 2));
			if (l < sizeof name) {
				memcpy(name, s + 2, l);
				name[l] = 0;
				t = __wctype_l(name, ct);
			}
			if (!t) { p->err = REG_ECTYPE; goto fail; }
			if ((p->flags & REG_ICASE) && (!strcmp(name, "upper") || !strcmp(name, "lower")))
				t = __wctype_l("alpha", ct);
			/* the class as ranges */
			{
				uint32_t x, y;
				for (x = 0; x < 0x110000; x = y + 1) {
					y = __iswctype_run(x, t, ct);
					if (y > 0x10ffff) y = 0x10ffff;
					if (__iswctype_l(x, t, ct) && ws_add(p, &w, x, y)) goto fail;
				}
			}
			p->s = e + 2;
			if (p->s[0] == '-' && p->s[1] != ']' && p->s[1]) { p->err = REG_ERANGE; goto fail; }
			continue;
		}
		if (s[0] == '[' && s[1] == '=') {
			if (collname(p, 1, a, &na)) goto fail;
			if (!__coll_valids(co, 1, a, na)) { p->err = REG_ECOLLATE; goto fail; }
			if (na == 1 && ws_add(p, &w, a[0], a[0])) goto fail;
			if (co->nrules) {
				/* the characters of its equivalence class, as ranges */
				/* (a block the table has no entries for is one class) */
				uint32_t x, y;
				for (x = 0; x < 0x110000; x = y + 1) {
					y = co->tablewc ? __tab_run(co->tablewc, x) : x;
					if (y > 0x10ffff) y = 0x10ffff;
					if (__coll_equivs(co, 1, a, na, &x, 1) && ws_add(p, &w, x, y)) goto fail;
				}
				if (me_mark(p, &me, 1, a, na)) goto fail;
			}
			continue;
		}
		if (brelem(p, &me, a, &na)) goto fail;
		if (p->s[0] == '-' && p->s[1] && p->s[1] != ']') {
			p->s++;
			if (p->s[0] == '[' && (p->s[1] == '=' || p->s[1] == ':')) { p->err = REG_ERANGE; goto fail; }
			if (brelem(p, &me, b, &nb)) goto fail;
			if (co->nrules && co->collseqwc) {
				/* in a locale with collation rules, by the collation
				 * sequence (as glibc); else by code point */
				uint32_t sc = __coll_seqs(co, 1, a, na), sd = __coll_seqs(co, 1, b, nb);
				struct wsadd wa;
				if (sc == ~0u || sd == ~0u) { p->err = REG_ECOLLATE; goto fail; }
				if (sc > sd) { p->err = REG_ERANGE; goto fail; }
				wa.p = p; wa.w = &w;
				if (__coll_seq_runs(co, sc, sd, ws_add_cb, &wa)) goto fail;
			} else {
				if (b[0] < a[0]) { p->err = REG_ERANGE; goto fail; }
				if (ws_add(p, &w, a[0], b[0])) goto fail;
			}
		} else if (na == 1) {
			if (ws_add(p, &w, a[0], a[0])) goto fail;
		} else
			if (me_mark(p, &me, 0, a, na)) goto fail;
	}
	ws_norm(p, &w);
	if ((p->flags & REG_ICASE) && ws_fold(p, &w)) goto fail;
	if (neg) {
		if (ws_invert(p, &w)) goto fail;
		if (p->flags & REG_NEWLINE) {
			WS t = { 0 };
			size_t k;
			/* remove '\n' */
			for (k = 0; k < w.n; k++) {
				uint32_t lo = w.r[k][0], hi = w.r[k][1];
				if (lo <= '\n' && hi >= '\n') {
					if (lo < '\n' && ws_add(p, &t, lo, '\n' - 1)) goto fail;
					if (hi > '\n' && ws_add(p, &t, '\n' + 1, hi)) goto fail;
				} else if (ws_add(p, &t, lo, hi)) goto fail;
			}
			free(w.r);
			w = t;
		}
	}
	n = u8node(p, &w);
	free(w.r);
	return me_finish(p, &me, neg, n);
fail:
	free(w.r);
	free(me.in);
	return 0;
}

static struct re_node *alt(P *p, int depth);

/* m and n of an interval; p->s just after "{" or "\{" */
static int interval(P *p, int *min, int *max)
{
	const unsigned char *s = p->s;
	long a = 0, b;
	int digits = 0;
	while (*s >= '0' && *s <= '9') { if (a <= RE_DUP_MAX) a = a * 10 + (*s - '0'); s++; digits++; }
	if (!digits && !(p->ere && *s == ',')) return REG_BADBR;
	b = a;
	if (*s == ',') {
		s++;
		if (*s >= '0' && *s <= '9') {
			b = 0;
			while (*s >= '0' && *s <= '9') { if (b <= RE_DUP_MAX) b = b * 10 + (*s - '0'); s++; }
		} else b = -1;
	}
	if (p->ere ? *s != '}' : !(s[0] == '\\' && s[1] == '}')) {
		/* "{1" at the end, "{1,a}" ... */
		const unsigned char *e = s;
		while (*e && (p->ere ? *e != '}' : !(e[0] == '\\' && e[1] == '}'))) e++;
		return *e ? REG_BADBR : REG_EBRACE;
	}
	s += p->ere ? 1 : 2;
	if (a > RE_DUP_MAX || b > RE_DUP_MAX || (b >= 0 && b < a)) return REG_BADBR;
	*min = (int)a; *max = (int)b;
	p->s = s;
	return 0;
}

static struct re_node *rep(P *p, struct re_node *a, int min, int max)
{
	struct re_node *n;
	int mod = 0;
	if (p->ere && *p->s == '?') { mod = 1; p->s++; }
	if (!(n = mk(p, &p->pa, N_REP, 1))) return 0;
	n->kid[0] = a;
	n->min = min; n->max = max;
	n->minimal = (unsigned char)((p->ere && (p->flags & REG_MINIMAL)) ? !mod : mod);
	if (n->minimal) p->g->minimal = 1;
	return n;
}

static struct re_node *cat(P *p, int depth)
{
	struct re_node **v = 0, **nv, *a, *n;
	size_t nv_n = 0, cap = 0, i;
	int start = 1;
	for (;;) {
		const unsigned char *s = p->s;
		int c = *s;
		a = 0;
		if (!c) break;
		if (p->ere) {
			if (c == '|' || (c == ')' && depth)) break;
		} else if (c == '\\' && (s[1] == '|' || (s[1] == ')' && depth))) break;
		else if (c == '\\' && s[1] == ')') { p->err = REG_EPAREN; goto fail; }

		/* atom */
		if (!p->ere && c == '^' && start && !nv_n) {
			/* an anchor; still at the start: a following '*' is literal */
			if (!(a = mk(p, &p->pa, N_BOL, 0))) goto fail;
			p->s++;
			goto push;
		}
		if (!p->ere && c == '$' && (!s[1] || (s[1] == '\\' && (s[2] == ')' || s[2] == '|')))) {
			a = mk(p, &p->pa, N_EOL, 0);
			p->s++;
		} else if (p->ere && c == '^') {
			a = mk(p, &p->pa, N_BOL, 0); p->s++;
		} else if (p->ere && c == '$') {
			a = mk(p, &p->pa, N_EOL, 0); p->s++;
		} else if (c == '.') {
			p->s++;
			a = p->u8 ? u8any(p) : anychar(p);
		} else if (c == '[') {
			p->s++;
			a = p->u8 ? u8bracket(p) : bracket(p);
		} else if (p->ere && (c == '*' || c == '+' || c == '?')) {
			p->err = REG_BADRPT; goto fail;
		} else if (p->ere && c == '{' && ((s[1] >= '0' && s[1] <= '9') || s[1] == ',')) {
			p->err = REG_BADRPT; goto fail;
		} else if ((p->ere && c == '(') || (!p->ere && c == '\\' && s[1] == '(')) {
			size_t gn = ++p->ngroups;
			p->s += p->ere ? 1 : 2;
			if (!(n = alt(p, depth + 1))) goto fail;
			if (p->ere ? *p->s != ')' : !(p->s[0] == '\\' && p->s[1] == ')')) { p->err = REG_EPAREN; goto fail; }
			p->s += p->ere ? 1 : 2;
			if (gn < 10) p->closed[gn] = 1;
			if (!(a = mk(p, &p->pa, N_GROUP, 1))) goto fail;
			a->kid[0] = n;
			a->val = (int)gn;
		} else if (c == '\\') {
			if (!s[1]) { p->err = REG_EESCAPE; goto fail; }
			if (s[1] >= '1' && s[1] <= '9') {
				if (!p->closed[s[1] - '0']) { p->err = REG_ESUBREG; goto fail; }
				if (!(a = mk(p, &p->pa, N_BREF, 0))) goto fail;
				a->val = s[1] - '0';
				p->g->bref = 1;
				p->g->refd |= 1u << a->val;
			} else if (strchr("<>bB`'", s[1])) {
				/* GNU extensions, as glibc: word and buffer assertions */
				static const char kinds[] = "<>bB`";
				if (!(a = mk(p, &p->pa, s[1] == '\'' ? N_EOL : N_BOL, 0))) goto fail;
				a->val = s[1] == '\'' ? A_BUFEND : (int)(strchr(kinds, s[1]) - kinds) + A_WORDSTART;
			} else if (strchr("wWsS", s[1])) {
				/* \w = [[:alnum:]_], \W, \s = [[:space:]], \S */
				static const char *const cls[] = { "[:alnum:]_]", "^[:alnum:]_]", "[:space:]]", "^[:space:]]" };
				static const char which[] = "wWsS";
				const unsigned char *save = p->s;
				p->s = (const unsigned char *)cls[strchr(which, s[1]) - which];
				a = p->u8 ? u8bracket(p) : bracket(p);
				p->s = save;
				if (!a) goto fail;
			} else if (!p->ere && (s[1] == '{' || s[1] == '?' || s[1] == '+') && start) {
				a = lit(p, s[1]);       /* nothing to repeat: literal */
			} else if (!p->ere && (s[1] == '{' || s[1] == '?' || s[1] == '+')) {
				p->err = REG_BADRPT; goto fail;
			} else if (p->u8 && s[1] >= 0x80) {
				p->s++;
				a = u8char(p);
				p->s--;   /* (the next line adds two) */
				p->s--;
			} else
				a = lit(p, s[1]);
			p->s += 2;
		} else if (!p->ere && c == '*' && start) {
			a = lit(p, '*'); p->s++;
		} else if (p->u8 && c >= 0x80) {
			a = u8char(p);
		} else {
			a = lit(p, c); p->s++;
		}
		if (!a) goto fail;

		/* duplication symbols */
		for (;;) {
			int min, max, e;
			s = p->s;
			if (s[0] == '*') { p->s++; a = rep(p, a, 0, -1); }
			else if (p->ere && s[0] == '+') { p->s++; a = rep(p, a, 1, -1); }
			else if (p->ere && s[0] == '?') { p->s++; a = rep(p, a, 0, 1); }
			else if (!p->ere && s[0] == '\\' && s[1] == '+') { p->s += 2; a = rep(p, a, 1, -1); }
			else if (!p->ere && s[0] == '\\' && s[1] == '?') { p->s += 2; a = rep(p, a, 0, 1); }
			else if ((p->ere && s[0] == '{' && ((s[1] >= '0' && s[1] <= '9') || s[1] == ',')) ||
			         (!p->ere && s[0] == '\\' && s[1] == '{')) {
				p->s += p->ere ? 1 : 2;
				if ((e = interval(p, &min, &max))) { p->err = e; goto fail; }
				a = rep(p, a, min, max);
			} else break;
			if (!a) goto fail;
		}
		start = 0;
	push:
		if (nv_n == cap) {
			cap = cap ? 2 * cap : 8;
			if (!(nv = realloc(v, cap * sizeof *v))) { p->err = REG_ESPACE; goto fail; }
			v = nv;
		}
		v[nv_n++] = a;
	}
	if (nv_n == 1) { a = v[0]; free(v); return a; }
	if (!(n = mk(p, &p->pa, nv_n ? N_CAT : N_EMPTY, (int)nv_n))) goto fail;
	for (i = 0; i < nv_n; i++) n->kid[i] = v[i];
	free(v);
	return n;
fail:
	free(v);
	return 0;
}

static struct re_node *alt(P *p, int depth)
{
	struct re_node *v[64], *a, *n;
	int k = 0, i;
	for (;;) {
		if (!(a = cat(p, depth))) return 0;
		if (k == 64) {             /* fold: (a|b|...) nests */
			if (!(n = mk(p, &p->pa, N_ALT, k))) return 0;
			for (i = 0; i < k; i++) n->kid[i] = v[i];
			v[0] = n; k = 1;
		}
		v[k++] = a;
		if (p->ere ? *p->s == '|' : (p->s[0] == '\\' && p->s[1] == '|')) p->s += p->ere ? 1 : 2;
		else break;
	}
	if (k == 1) return v[0];
	if (!(n = mk(p, &p->pa, N_ALT, k))) return 0;
	for (i = 0; i < k; i++) n->kid[i] = v[i];
	return n;
}

/* ---- expansion of repetitions into the final tree ---- */

static struct re_node *expand(P *p, struct re_node *t);

static struct re_node *copy1(P *p, struct re_node *t, int type, int nkid)
{
	struct re_node *n;
	if (++p->nodes > MAXNODES) { p->err = REG_ESPACE; return 0; }
	if (!(n = mk(p, &p->g->arena, type, nkid))) return 0;
	n->val = t ? t->val : 0;
	return n;
}

static struct re_node *expand_rep(P *p, struct re_node *t)
{
	struct re_node *x = t->kid[0], *n, *tail = 0, *c;
	int m = t->min, M = t->max, k, i, nk;
	if (M == 0) return copy1(p, 0, N_EMPTY, 0);
	if (M < 0) {
		if (!(tail = copy1(p, 0, N_STAR, 1)) || !(c = expand(p, x))) return 0;
		c->rep = 1;
		tail->kid[0] = c;
		tail->minimal = t->minimal;
	} else {
		for (k = M - m; k > 0; k--) {
			struct re_node *inner, *o;
			if (!(c = expand(p, x))) return 0;
			c->rep = 1;
			if (tail) {
				if (!(inner = copy1(p, 0, N_CAT, 2))) return 0;
				inner->kid[0] = c; inner->kid[1] = tail;
				inner->minimal = t->minimal;
			} else inner = c;
			if (!(o = copy1(p, 0, N_OPT, 1))) return 0;
			o->kid[0] = inner;
			o->minimal = t->minimal;
			tail = o;
		}
	}
	if (!m) return tail;
	nk = m + (tail != 0);
	if (nk == 1) { if (!(c = expand(p, x))) return 0; c->rep = 1; return c; }
	if (!(n = copy1(p, 0, N_CAT, nk))) return 0;
	n->minimal = t->minimal;
	for (i = 0; i < m; i++) {
		if (!(c = expand(p, x))) return 0;
		c->rep = 1;
		n->kid[i] = c;
	}
	if (tail) n->kid[m] = tail;
	return n;
}

static struct re_node *expand(P *p, struct re_node *t)
{
	struct re_node *n;
	int i;
	if (t->type == N_REP) return expand_rep(p, t);
	if (!(n = copy1(p, t, t->type, t->nkid))) return 0;
	n->minimal = t->minimal;
	for (i = 0; i < t->nkid; i++)
		if (!(n->kid[i] = expand(p, t->kid[i]))) return 0;
	if (t->type == N_GROUP && n->kid[0]->minimal) n->minimal = 1;
	return n;
}

/* group ranges, length bounds */
static void annotate(struct re_node *n, unsigned refd)
{
	int i, lo = INT_MAX, hi = 0;
	long mn = 0, mx = 0;
	for (i = 0; i < n->nkid; i++) {
		annotate(n->kid[i], refd);
		n->hasbref |= n->kid[i]->hasbref;
		n->hasref |= n->kid[i]->hasref;
	}
	if (n->type == N_BREF) n->hasbref = 1;
	if (n->type == N_GROUP && n->val < 10 && (refd >> n->val & 1)) n->hasref = 1;
	for (i = 0; i < n->nkid; i++)
		if (n->kid[i]->ghi > n->kid[i]->glo) {
			if (n->kid[i]->glo < lo) lo = n->kid[i]->glo;
			if (n->kid[i]->ghi > hi) hi = n->kid[i]->ghi;
		}
	if (n->type == N_GROUP) { lo = n->val; if (hi < n->val + 1) hi = n->val + 1; }
	if (lo == INT_MAX) lo = hi = 0;
	n->glo = lo; n->ghi = hi;
	switch (n->type) {
	case N_CHAR: case N_SET: mn = mx = 1; break;
	case N_BOL: case N_EOL: case N_EMPTY: break;
	case N_BREF: mx = -1; break;
	case N_GROUP: mn = n->kid[0]->minlen; mx = n->kid[0]->maxlen; break;
	case N_CAT:
		for (i = 0; i < n->nkid; i++) {
			mn += n->kid[i]->minlen;
			if (mx >= 0) mx = n->kid[i]->maxlen < 0 ? -1 : mx + n->kid[i]->maxlen;
		}
		break;
	case N_ALT:
		mn = LONG_MAX;
		for (i = 0; i < n->nkid; i++) {
			if (n->kid[i]->minlen < mn) mn = n->kid[i]->minlen;
			if (mx >= 0) mx = n->kid[i]->maxlen < 0 ? -1 : (n->kid[i]->maxlen > mx ? n->kid[i]->maxlen : mx);
		}
		break;
	case N_STAR: mx = n->kid[0]->maxlen == 0 ? 0 : -1; break;
	case N_OPT: mx = n->kid[0]->maxlen; break;
	}
	if (mn > INT_MAX) mn = INT_MAX;
	if (mx > INT_MAX) mx = -1;
	n->minlen = (int)mn; n->maxlen = (int)mx;
}

/* ---- program ---- */

typedef struct { struct re_inst *v; int n, cap; int err, anyset; } Code;

static int emit(Code *c, int op, int x, int y)
{
	if (c->n == c->cap) {
		struct re_inst *nv;
		c->cap = c->cap ? 2 * c->cap : 64;
		if (!(nv = realloc(c->v, (size_t)c->cap * sizeof *nv))) { c->err = 1; c->cap = c->n; return -1; }
		c->v = nv;
	}
	c->v[c->n].op = (unsigned char)op; c->v[c->n].x = x; c->v[c->n].y = y;
	return c->n++;
}

static void gen(Code *c, struct re_node *n)
{
	int i, s, j, *jl;
	n->entry = c->n;
	switch (n->type) {
	case N_CHAR: emit(c, I_CHAR, n->val, 0); break;
	case N_SET: emit(c, I_SET, n->val, 0); break;
	case N_BOL: emit(c, I_BOL, n->val, 0); break;
	case N_EOL: emit(c, I_EOL, n->val, 0); break;
	case N_EMPTY: break;
	case N_BREF:                /* any string: an over-approximation */
		s = emit(c, I_SPLIT, 0, 0);
		emit(c, I_SET, c->anyset, 0);
		emit(c, I_JMP, s, 0);
		if (!c->err) { c->v[s].x = s + 1; c->v[s].y = c->n; }
		break;
	case N_GROUP: gen(c, n->kid[0]); break;
	case N_CAT: for (i = 0; i < n->nkid; i++) gen(c, n->kid[i]); break;
	case N_ALT:
		if (!(jl = malloc((size_t)n->nkid * sizeof *jl))) { c->err = 1; break; }
		for (i = 0; i < n->nkid - 1; i++) {
			s = emit(c, I_SPLIT, 0, 0);
			if (s >= 0) c->v[s].x = s + 1;
			gen(c, n->kid[i]);
			jl[i] = emit(c, I_JMP, 0, 0);
			if (s >= 0) c->v[s].y = c->n;
		}
		gen(c, n->kid[i]);
		if (!c->err) for (j = 0; j < n->nkid - 1; j++) c->v[jl[j]].x = c->n;
		free(jl);
		break;
	case N_STAR:
		s = emit(c, I_SPLIT, 0, 0);
		gen(c, n->kid[0]);
		emit(c, I_JMP, s, 0);
		if (!c->err) { c->v[s].x = s + 1; c->v[s].y = c->n; }
		break;
	case N_OPT:
		s = emit(c, I_SPLIT, 0, 0);
		gen(c, n->kid[0]);
		if (!c->err) { c->v[s].x = s + 1; c->v[s].y = c->n; }
		break;
	}
	n->end = c->n;
}

static int preds(struct re_prog *g)
{
	int n = g->ninst, i, *cnt;
	struct re_inst *v = g->inst;
	if (!(cnt = calloc((size_t)n + 2, sizeof *cnt))) return -1;
	for (i = 0; i < n; i++) {
		if (v[i].op == I_SPLIT) { cnt[v[i].x]++; cnt[v[i].y]++; }
		else if (v[i].op == I_JMP) cnt[v[i].x]++;
		else if (v[i].op == I_BOL || v[i].op == I_EOL) cnt[i + 1]++;
	}
	if (!(g->epredx = malloc(((size_t)n + 2) * sizeof(int)))) { free(cnt); return -1; }
	g->epredx[0] = 0;
	for (i = 0; i <= n; i++) g->epredx[i + 1] = g->epredx[i] + cnt[i];
	if (!(g->epred = malloc((size_t)g->epredx[n + 1] * sizeof(int) + 1))) { free(cnt); return -1; }
	memset(cnt, 0, ((size_t)n + 2) * sizeof *cnt);
#define ADD(t, q) (g->epred[g->epredx[t] + cnt[t]++] = (q))
	for (i = 0; i < n; i++) {
		if (v[i].op == I_SPLIT) { ADD(v[i].x, i); ADD(v[i].y, i); }
		else if (v[i].op == I_JMP) ADD(v[i].x, i);
		else if (v[i].op == I_BOL || v[i].op == I_EOL) ADD(i + 1, i);
	}
#undef ADD
	free(cnt);
	return 0;
}

static void freeprog(struct re_prog *g)
{
	if (!g) return;
	arena_free(g->arena);
	free(g->inst);
	free(g->sets);
	free(g->epred);
	free(g->epredx);
	free(g);
}

static int compile(regex_t *__RESTRICT re, const char *__RESTRICT pat, int cflags, const struct __lcat *mbcs)
{
	P p;
	struct re_node *t;
	struct re_prog *g = calloc(1, sizeof *g);
	Code c = { 0 };
	if (!g) return REG_ESPACE;
	memset(&p, 0, sizeof p);
	p.s = (const unsigned char *)pat;
	p.flags = cflags;
	p.ere = !!(cflags & REG_EXTENDED);
	p.g = g;
	p.u8 = CURRENT_UTF8 || mbcs;
	g->u8 = p.u8;
	g->mbcs = mbcs;
	g->flags = cflags;
	t = alt(&p, 0);
	if (t && *p.s) p.err = p.ere ? REG_BADPAT : REG_EPAREN;  /* stray "\)" */
	if (!p.err && t) t = expand(&p, t);
	arena_free(p.pa);
	if (p.err || !t) {
		freeprog(g);
		return p.err ? p.err : REG_ESPACE;
	}
	annotate(t, g->refd);
	g->root = t;
	g->nsub = p.ngroups;
	g->anchored = !(cflags & REG_NEWLINE) &&
		((t->type == N_BOL && !t->val) || (t->type == N_CAT && t->kid[0]->type == N_BOL && !t->kid[0]->val));
	if (g->bref) {
		if ((c.anyset = newset(&p)) < 0) { freeprog(g); return REG_ESPACE; }
		memset(g->sets[c.anyset], 0xff, 32);
	}
	gen(&c, t);
	emit(&c, I_MATCH, 0, 0);
	g->inst = c.v;
	g->ninst = c.n;
	if (c.err || preds(g)) { freeprog(g); return REG_ESPACE; }
	re->re_nsub = p.ngroups;
	re->__re_prog = g;
	re->__re_flags = cflags;
	return 0;
}

void regfree(regex_t *re)
{
	freeprog(re->__re_prog);
	re->__re_prog = 0;
}

int regcomp(regex_t *__RESTRICT re, const char *__RESTRICT pat, int cflags)
{
	const struct __lcat *ct = CURRENT_CTYPE;
	char *u;
	int r;
	if (ct->cs != CS_MB)
		return compile(re, pat, cflags, 0);
	/* EUC-JP, SHIFT_JIS, GB18030 ...: as UTF-8 (re_mb.c) */
	if (!(u = __re_to_u8(pat, strlen(pat), ct, 0, 0)))
		return REG_ESPACE;
	r = compile(re, u, cflags, ct);
	free(u);
	return r;
}
