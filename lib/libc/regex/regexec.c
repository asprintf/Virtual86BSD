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
#include <stdint.h>
#include <ctype.h>
#include "regex_impl.h"

/* ---- scratch memory with mark/release ---- */

struct chunk { struct chunk *prev; size_t used, cap; };

typedef struct {
	const struct re_prog *g;
	const unsigned char *s;
	long lo, hi;                /* the part of the string searched */
	int eflags;
	int err;
	regmatch_t *cap;            /* nsub + 1 */
	struct chunk *mem;
	/* simulation sets */
	int *dense, *sparse, *stk;
	long *start;
	int *dense2, *sparse2;
	long *start2;
	long depth;                 /* backtracking depth */
} X;

typedef struct { struct chunk *c; size_t used; } Mark;

static Mark mark(X *x) { Mark m = { x->mem, x->mem ? x->mem->used : 0 }; return m; }

static void release(X *x, Mark m)
{
	while (x->mem != m.c) { struct chunk *p = x->mem->prev; free(x->mem); x->mem = p; }
	if (x->mem) x->mem->used = m.used;
}

static void *salloc(X *x, size_t n)
{
	struct chunk *c = x->mem;
	n = (n + 15) & ~(size_t)15;
	if (!c || c->cap - c->used < n) {
		size_t cap = n > 65536 ? n : 65536;
		if (!(c = malloc(sizeof *c + 16 + cap))) { x->err = REG_ESPACE; return 0; }
		c->prev = x->mem; c->used = 0; c->cap = cap;
		x->mem = c;
	}
	c->used += n;
	return (char *)(c + 1) + 16 + c->used - n;
}

/* ---- position sets: bits for positions [base, last] ---- */

typedef uint64_t W;
typedef struct { long base, last; W *w; } BS;

static int bs_init(X *x, BS *b, long base, long last)
{
	size_t n = (size_t)(last - base) / 64 + 1;
	b->base = base; b->last = last;
	if (!(b->w = salloc(x, n * sizeof(W)))) return -1;
	memset(b->w, 0, n * sizeof(W));
	return 0;
}

static void bs_set(BS *b, long p) { b->w[(p - b->base) >> 6] |= (W)1 << ((p - b->base) & 63); }

static int bs_has(const BS *b, long p)
{
	if (p < b->base || p > b->last) return 0;
	return (int)((b->w[(p - b->base) >> 6] >> ((p - b->base) & 63)) & 1);
}

/* the 64 bits for positions [p, p+63] */
static W bs_get(const BS *b, long p)
{
	long o = p - b->base, wi;
	W r;
	if (p > b->last || o + 63 < 0) return 0;
	if (o < 0) {
		r = b->w[0] << (-o);
	} else {
		wi = o >> 6;
		r = b->w[wi] >> (o & 63);
		if ((o & 63) && (wi + 1) * 64 <= b->last - b->base) r |= b->w[wi + 1] << (64 - (o & 63));
	}
	if (b->last - p < 63) r &= ((W)1 << (b->last - p + 1)) - 1;
	return r;
}

static int lowbit(W w) { int k = 0; while (!(w & 1)) { w >>= 1; k++; } return k; }
static int highbit(W w) { int k = 63; while (!(w >> 63)) { w <<= 1; k--; } return k; }

/* ---- instructions ---- */

static int bol(const X *x, long p)
{
	if (p == 0 && !(x->eflags & REG_NOTBOL)) return 1;
	return (x->g->flags & REG_NEWLINE) && p > 0 && x->s[p - 1] == '\n';
}

static int eol(const X *x, long p)
{
	if (p == x->hi && !(x->eflags & REG_NOTEOL)) return 1;
	return (x->g->flags & REG_NEWLINE) && p < x->hi && x->s[p] == '\n';
}

/* a byte of a word: alphanumeric or '_' (in UTF-8, every non-ASCII byte) */
static int wordc(const X *x, long p)
{
	unsigned c = x->s[p];
	return c == '_' || (c >= 0x80 && x->g->u8) || isalnum((int)c);
}

/* the zero-width assertions: op I_BOL or I_EOL, of the given kind */
static int at(const X *x, int op, int kind, long p)
{
	int a, b;
	if (op == I_EOL)
		return kind == A_BUFEND ? p == x->hi : eol(x, p);
	if (kind == A_LINE)
		return bol(x, p);
	if (kind == A_BUFSTART)
		return p == 0;
	a = p > 0 && wordc(x, p - 1);
	b = p < x->hi && wordc(x, p);
	switch (kind) {
	case A_WORDSTART: return !a && b;
	case A_WORDEND: return a && !b;
	case A_BOUND: return a != b;
	default: return a == b;
	}
}

static int consumes(const struct re_inst *i) { return i->op == I_CHAR || i->op == I_SET; }

static int matches(const X *x, const struct re_inst *i, int c)
{
	if (i->op == I_CHAR) return i->x == c;
	return (x->g->sets[i->x][c >> 3] >> (c & 7)) & 1;
}

/* ---- phase 1: leftmost match, longest end ---- */

/* add pc and its epsilon closure at position p to the list (dense/sparse,
 * with start positions); MATCH reached updates the best match */
static void add(X *x, int *dense, int *sparse, long *start, int *n, int pc, long p, long st,
                long *bs, long *be)
{
	const struct re_inst *v = x->g->inst;
	int sp = 0, q;
	x->stk[sp++] = pc;
	while (sp) {
		q = x->stk[--sp];
		if (sparse[q] < *n && dense[sparse[q]] == q) continue;
		sparse[q] = *n; dense[*n] = q; start[*n] = st; (*n)++;
		switch (v[q].op) {
		case I_SPLIT: x->stk[sp++] = v[q].y; x->stk[sp++] = v[q].x; break;
		case I_JMP: x->stk[sp++] = v[q].x; break;
		case I_BOL: case I_EOL: if (at(x, v[q].op, v[q].x, p)) x->stk[sp++] = q + 1; break;
		case I_MATCH:
			if (*bs < 0 || st < *bs || (st == *bs && p > *be)) { *bs = st; *be = p; }
			break;
		}
	}
}

static int phase1(X *x, long *so, long *eo, int any)
{
	const struct re_inst *v = x->g->inst;
	int n = 0, n2, k, *d = x->dense, *sp = x->sparse, *d2 = x->dense2, *sp2 = x->sparse2, *ti;
	long *st = x->start, *st2 = x->start2, *tl, p, bs = -1, be = -1;
	for (p = x->lo; ; p++) {
		if (bs < 0 && (!x->g->anchored || p == 0))
			add(x, d, sp, st, &n, 0, p, p, &bs, &be);
		if (bs >= 0 && any) break;
		if (!n) {
			if (bs >= 0 || (x->g->anchored && p >= 0)) break;
			if (p >= x->hi) break;
			continue;
		}
		if (p >= x->hi) break;
		n2 = 0;
		for (k = 0; k < n; k++) {
			const struct re_inst *i = v + d[k];
			if (bs >= 0 && st[k] > bs) break;
			if (consumes(i) && matches(x, i, x->s[p]))
				add(x, d2, sp2, st2, &n2, d[k] + 1, p + 1, st[k], &bs, &be);
		}
		ti = d; d = d2; d2 = ti; ti = sp; sp = sp2; sp2 = ti; tl = st; st = st2; st2 = tl;
		n = n2;
	}
	*so = bs; *eo = be;
	return bs >= 0;
}

/* ---- fragment simulation ---- */

/* positions where control leaves [entry, end) when entered at i */
static int ends(X *x, BS *r, int entry, int end, long i)
{
	const struct re_inst *v = x->g->inst;
	int n = 0, n2, k, sp, q, *t;
	int *cur = x->dense, *curs = x->sparse, *nxt = x->dense2, *nxts = x->sparse2;
	long p = i, nw = (x->hi - i) / 64 + 1;
	/* the words are cleared as the simulation advances */
	r->base = i; r->last = i;
	if (!(r->w = salloc(x, (size_t)nw * sizeof(W)))) return -1;
	r->w[0] = 0;
#define INS(list, lsp, cnt, pc0, pos) do { \
		sp = 0; x->stk[sp++] = (pc0); \
		while (sp) { \
			q = x->stk[--sp]; \
			if (q == end) { bs_set(r, (pos)); continue; } \
			if (lsp[q] < cnt && list[lsp[q]] == q) continue; \
			lsp[q] = cnt; list[cnt++] = q; \
			switch (v[q].op) { \
			case I_SPLIT: x->stk[sp++] = v[q].y; x->stk[sp++] = v[q].x; break; \
			case I_JMP: x->stk[sp++] = v[q].x; break; \
			case I_BOL: case I_EOL: if (at(x, v[q].op, v[q].x, (pos))) x->stk[sp++] = q + 1; break; \
			} \
		} } while (0)
	INS(cur, curs, n, entry, p);
	while (n && p < x->hi) {
		n2 = 0;
		if (!((p + 1 - i) & 63)) r->w[(p + 1 - i) >> 6] = 0;
		r->last = p + 1;
		for (k = 0; k < n; k++) {
			q = cur[k];
			if (consumes(v + q) && matches(x, v + q, x->s[p])) {
				int pc1 = q + 1;
				INS(nxt, nxts, n2, pc1, p + 1);
			}
		}
		t = cur; cur = nxt; nxt = t; t = curs; curs = nxts; nxts = t;
		n = n2;
		p++;
	}
#undef INS
	return 0;
}

/* positions p in [i, A.last] from which (entry, p) reaches (end, a) for
 * some a in A */
static int starts(X *x, BS *r, int entry, int end, const BS *A, long i)
{
	const struct re_inst *v = x->g->inst;
	const int *pr = x->g->epred, *px = x->g->epredx;
	int *cur = x->dense, *curs = x->sparse, *prv = x->dense2, *prvs = x->sparse2, *t;
	int n = 0, np = 0, k, j, m, q, sp;
	long p, top = A->last;
	if (bs_init(x, r, i, top < i ? i : top)) return -1;
	for (p = top; p >= i; p--) {
		n = 0;
#define PUT(pc) do { int pc_ = (pc); \
		if (!(curs[pc_] < n && cur[curs[pc_]] == pc_)) { curs[pc_] = n; cur[n++] = pc_; x->stk[sp++] = pc_; } \
	} while (0)
		sp = 0;
		if (bs_has(A, p)) PUT(end);
		if (p < top)
			for (k = 0; k < np; k++) {
				q = prv[k] - 1;
				if (q >= entry && q < end && consumes(v + q) && matches(x, v + q, x->s[p])) PUT(q);
			}
		while (sp) {
			m = x->stk[--sp];
			for (j = px[m]; j < px[m + 1]; j++) {
				q = pr[j];
				if (q < entry || q >= end) continue;
				if ((v[q].op == I_BOL || v[q].op == I_EOL) && !at(x, v[q].op, v[q].x, p)) continue;
				PUT(q);
			}
		}
#undef PUT
		if (curs[entry] < n && cur[curs[entry]] == entry) bs_set(r, p);
		t = cur; cur = prv; prv = t; t = curs; curs = prvs; prvs = t;
		np = n;
		if (!n && p - 1 < A->base) break;   /* nothing can be reached below */
	}
	return 0;
}

/* ---- phase 2: subexpressions, top-down ---- */

static void reset(X *x, const struct re_node *n)
{
	int g;
	for (g = n->glo; g < n->ghi; g++) x->cap[g].rm_so = x->cap[g].rm_eo = -1;
}

/* the preferred element of E ∩ R (R may be null), not skip, or -1 */
static long pick(const BS *E, const BS *R, int shortest, long skip)
{
	long lo = E->base, hi = E->last, p;
	W w;
	if (R) {
		if (R->base > lo) lo = R->base;
		if (R->last < hi) hi = R->last;
	}
	if (lo > hi) return -1;
	if (shortest) {
		for (p = lo; p <= hi; p += 64) {
			w = bs_get(E, p) & (R ? bs_get(R, p) : ~(W)0);
			if (skip >= p && skip < p + 64) w &= ~((W)1 << (skip - p));
			if (hi - p < 63) w &= ((W)1 << (hi - p + 1)) - 1;
			if (w) return p + lowbit(w);
		}
	} else {
		for (p = hi - 63; p > lo - 64; p -= 64) {
			long q = p < lo ? lo : p;
			w = bs_get(E, q) & (R ? bs_get(R, q) : ~(W)0);
			if (skip >= q && skip < q + 64) w &= ~((W)1 << (skip - q));
			if (hi - q < 63) w &= ((W)1 << (hi - q + 1)) - 1;
			if (w) return q + highbit(w);
			if (q == lo) break;
		}
	}
	return -1;
}

static int single(X *x, BS *b, long p)
{
	if (bs_init(x, b, p, p)) return -1;
	bs_set(b, p);
	return 0;
}

static long solve(X *x, const struct re_node *n, long i, const BS *A)
{
	Mark m;
	BS E, R, B;
	long k, e;
	int t;
	if (x->err) return -1;
	if (n->rep) reset(x, n);
	switch (n->type) {
	case N_CHAR: case N_SET: return i + 1;
	case N_BOL: case N_EOL: case N_EMPTY: return i;
	case N_GROUP:
		e = solve(x, n->kid[0], i, A);
		if (e >= 0) { x->cap[n->val].rm_so = i; x->cap[n->val].rm_eo = e; }
		return e;
	case N_ALT:
		for (t = 0; t < n->nkid - 1; t++) {
			m = mark(x);
			k = ends(x, &E, n->kid[t]->entry, n->kid[t]->end, i) ? -1 : pick(&E, A, 0, -1);
			release(x, m);
			if (x->err) return -1;
			if (k >= 0) break;
		}
		return solve(x, n->kid[t], i, A);
	case N_CAT:
		for (t = 0; t < n->nkid - 1; t++) {
			m = mark(x);
			if (ends(x, &E, n->kid[t]->entry, n->kid[t]->end, i) ||
			    starts(x, &R, n->kid[t + 1]->entry, n->end, A, i)) return -1;
			k = pick(&E, &R, n->kid[t]->minimal, -1);
			if (k < 0 || single(x, &B, k)) { x->err = x->err ? x->err : REG_ESPACE; return -1; }
			solve(x, n->kid[t], i, &B);
			release(x, m);
			i = k;
		}
		return solve(x, n->kid[t], i, A);
	case N_STAR:
		m = mark(x);
		if (starts(x, &R, n->entry, n->end, A, i)) return -1;
		for (t = 0; ; t++) {
			Mark m2 = mark(x);
			if (n->minimal && bs_has(A, i)) break;
			if (ends(x, &E, n->kid[0]->entry, n->kid[0]->end, i)) return -1;
			k = pick(&E, &R, n->minimal, i);
			if (k < 0) {
				/* no further non-empty iteration: i is in A. A body that
				 * can match the empty string takes part once, as in
				 * (a*)* matching "" with \1 = (0,0). */
				if (!t && bs_has(&E, i) && !single(x, &B, i)) solve(x, n->kid[0], i, &B);
				release(x, m2);
				break;
			}
			if (single(x, &B, k)) return -1;
			solve(x, n->kid[0], i, &B);
			release(x, m2);
			i = k;
		}
		release(x, m);
		return i;
	case N_OPT:
		if (n->minimal && bs_has(A, i)) return i;
		m = mark(x);
		if (ends(x, &E, n->kid[0]->entry, n->kid[0]->end, i)) return -1;
		k = pick(&E, A, 0, bs_has(A, i) ? i : -1);
		t = k < 0 && bs_has(A, i) && !bs_has(&E, i);
		release(x, m);
		if (t) return i;
		return solve(x, n->kid[0], i, A);
	}
	return -1;
}

/* ---- back-references: backtracking on the tree ---- */

enum { K_CAT, K_STAR, K_GROUP };
struct kont { int kind, idx; const struct re_node *n; long t, gs; struct kont *next; };

static int bt(X *x, const struct re_node *n, long i, long t, struct kont *k, int known);

static int cont(X *x, struct kont *k, long i);

/* candidate ends t1 for kid c of a concatenation (or for an iteration
 * of a repetition): c can match [i, t1) and the fragment [rentry, rend)
 * can then match [t1, t) -- both judged by the program, in which
 * back-references match anything */
static int cands(X *x, BS *C, const struct re_node *c, int rentry, int rend, long i, long t)
{
	BS E, R, T;
	long p;
	if (ends(x, &E, c->entry, c->end, i) || single(x, &T, t) || starts(x, &R, rentry, rend, &T, i))
		return -1;
	if (bs_init(x, C, i, t)) return -1;
	for (p = i; p <= t && p <= E.last; p++)
		if (bs_has(&E, p) && bs_has(&R, p)) bs_set(C, p);
	return 0;
}

static int catrest(X *x, const struct re_node *n, int idx, long i, long t, struct kont *next)
{
	const struct re_node *c = n->kid[idx];
	struct kont f;
	BS C;
	long t1;
	Mark m;
	if (idx == n->nkid - 1) return bt(x, c, i, t, next, 0);
	m = mark(x);
	if (cands(x, &C, c, n->kid[idx + 1]->entry, n->end, i, t)) return 0;
	f.kind = K_CAT; f.n = n; f.idx = idx + 1; f.t = t; f.next = next;
	if (c->minimal) {
		for (t1 = i; t1 <= t; t1++) if (bs_has(&C, t1) && bt(x, c, i, t1, &f, 1)) return 1;
	} else {
		for (t1 = t; t1 >= i; t1--) if (bs_has(&C, t1) && bt(x, c, i, t1, &f, 1)) return 1;
	}
	release(x, m);
	return 0;
}

static int cont(X *x, struct kont *k, long i)
{
	regmatch_t old;
	if (!k) return 1;
	switch (k->kind) {
	case K_CAT: return catrest(x, k->n, k->idx, i, k->t, k->next);
	case K_STAR: return bt(x, k->n, i, k->t, k->next, 0);
	case K_GROUP:
		old = x->cap[k->n->val];
		x->cap[k->n->val].rm_so = k->gs;
		x->cap[k->n->val].rm_eo = i;
		if (cont(x, k->next, i)) return 1;
		x->cap[k->n->val] = old;
		return 0;
	}
	return 0;
}

static int bref(X *x, int g, long i, long t)
{
	long so = x->cap[g].rm_so, l = x->cap[g].rm_eo - so, j;
	if (so < 0 || l != t - i) return 0;
	if (x->g->flags & REG_ICASE) {
		for (j = 0; j < l; j++) if (tolower(x->s[so + j]) != tolower(x->s[i + j])) return 0;
		return 1;
	}
	return !memcmp(x->s + so, x->s + i, (size_t)l);
}

static int bt1(X *x, const struct re_node *n, long i, long t, struct kont *k, int known)
{
	struct kont f;
	long len = t - i, t1;
	int j;
	switch (n->type) {
	case N_CHAR:
		return len == 1 && x->s[i] == n->val && cont(x, k, t);
	case N_SET:
		return len == 1 && ((x->g->sets[n->val][x->s[i] >> 3] >> (x->s[i] & 7)) & 1) && cont(x, k, t);
	case N_BOL: return !len && at(x, I_BOL, n->val, i) && cont(x, k, t);
	case N_EOL: return !len && at(x, I_EOL, n->val, i) && cont(x, k, t);
	case N_EMPTY: return !len && cont(x, k, t);
	case N_BREF: return bref(x, n->val, i, t) && cont(x, k, t);
	case N_GROUP:
		f.kind = K_GROUP; f.n = n; f.gs = i; f.next = k;
		return bt(x, n->kid[0], i, t, &f, known);
	case N_CAT: return catrest(x, n, 0, i, t, k);
	case N_ALT:
		for (j = 0; j < n->nkid; j++) if (bt(x, n->kid[j], i, t, k, 0)) return 1;
		return 0;
	case N_STAR:
		if (!len) return bt(x, n->kid[0], i, i, k, 0) || cont(x, k, t);
		f.kind = K_STAR; f.n = n; f.t = t; f.next = k;
		{
			BS C;
			Mark m = mark(x);
			if (cands(x, &C, n->kid[0], n->entry, n->end, i, t)) return 0;
			if (n->minimal) {
				for (t1 = i + 1; t1 <= t; t1++) if (bs_has(&C, t1) && bt(x, n->kid[0], i, t1, &f, 1)) return 1;
			} else {
				for (t1 = t; t1 > i; t1--) if (bs_has(&C, t1) && bt(x, n->kid[0], i, t1, &f, 1)) return 1;
			}
			release(x, m);
		}
		return 0;
	case N_OPT:
		if (!len) return bt(x, n->kid[0], i, i, k, 0) || cont(x, k, t);
		return bt(x, n->kid[0], i, t, k, known);
	}
	return 0;
}

static int bt(X *x, const struct re_node *n, long i, long t, struct kont *k, int known)
{
	long len = t - i;
	regmatch_t *save = 0;
	int r, ng = n->ghi - n->glo;
	Mark m;
	if (x->err || len < n->minlen || (n->maxlen >= 0 && len > n->maxlen)) return 0;
	if (++x->depth > 30000) { x->err = REG_ESPACE; return 0; }
	m = mark(x);
	if (n->rep && ng) {
		if (!(save = salloc(x, (size_t)ng * sizeof *save))) { x->depth--; return 0; }
		memcpy(save, x->cap + n->glo, (size_t)ng * sizeof *save);
		reset(x, n);
	}
	if (!n->hasbref && !(n->type == N_GROUP ? n->kid[0]->hasref : n->hasref)) {
		/* no back-reference inside, none to a group inside: the program decides, solve() picks
		 * the subexpressions (needed only for groups below n) */
		BS E, T;
		regmatch_t *sv = 0;
		r = 0;
		if (known || (!ends(x, &E, n->entry, n->end, i) && bs_has(&E, t))) {
			if (ng && !save && (sv = salloc(x, (size_t)ng * sizeof *sv)))
				memcpy(sv, x->cap + n->glo, (size_t)ng * sizeof *sv);
			if (n->type == N_GROUP && n->kid[0]->ghi == n->kid[0]->glo) {
				x->cap[n->val].rm_so = i; x->cap[n->val].rm_eo = t;
				r = cont(x, k, t);
			} else if (n->ghi == n->glo)
				r = cont(x, k, t);
			else if (!single(x, &T, t) && solve(x, n, i, &T) == t)
				r = cont(x, k, t);
			if (!r && sv) memcpy(x->cap + n->glo, sv, (size_t)ng * sizeof *sv);
		}
	} else
		r = bt1(x, n, i, t, k, known);
	if (!r && save) memcpy(x->cap + n->glo, save, (size_t)ng * sizeof *save);
	if (!r) release(x, m);
	x->depth--;
	return r;
}

/* ---- regexec ---- */

static int exec(const regex_t *__RESTRICT re, const char *__RESTRICT str, size_t nmatch,
            regmatch_t *__RESTRICT pm, int eflags)
{
	const struct re_prog *g = re->__re_prog;
	X x;
	size_t ncap = g->nsub + 1, i, ni;
	long so = -1, eo = -1, s, t;
	int found = 0, want;
	memset(&x, 0, sizeof x);
	x.g = g;
	x.s = (const unsigned char *)str;
	x.eflags = eflags;
	if (eflags & REG_STARTEND) { x.lo = pm[0].rm_so; x.hi = pm[0].rm_eo; }
	else { x.lo = 0; x.hi = (long)strlen(str); }
	if (g->flags & REG_NOSUB) nmatch = 0;
	want = nmatch > 0;
	if (!(x.cap = malloc(ncap * sizeof *x.cap))) return REG_ESPACE;
	for (i = 0; i < ncap; i++) x.cap[i].rm_so = x.cap[i].rm_eo = -1;

	ni = (size_t)g->ninst + 1;
	x.dense = malloc(ni * sizeof(int)); x.sparse = malloc(ni * sizeof(int));
	x.dense2 = malloc(ni * sizeof(int)); x.sparse2 = malloc(ni * sizeof(int));
	x.stk = malloc(2 * ni * sizeof(int));
	x.start = malloc(ni * sizeof(long)); x.start2 = malloc(ni * sizeof(long));
	if (!x.dense || !x.sparse || !x.dense2 || !x.sparse2 || !x.stk || !x.start || !x.start2)
		x.err = REG_ESPACE;
	else {
		/* the sparse sets are used without clearing; keep the tools quiet */
		memset(x.sparse, 0, ni * sizeof(int)); memset(x.sparse2, 0, ni * sizeof(int));
		if (g->bref) {
			BS E, S, A;
			Mark m;
			/* where a match can start at all, by the program */
			if (bs_init(&x, &A, x.lo, x.hi) == 0) memset(A.w, 0xff, (size_t)((x.hi - x.lo) / 64 + 1) * sizeof(W));
			if (x.err || starts(&x, &S, g->root->entry, g->root->end, &A, x.lo)) x.err = REG_ESPACE;
			for (s = x.lo; s <= x.hi && !found && !x.err; s++) {
				if (g->anchored && s > 0) break;
				if (!bs_has(&S, s)) continue;
				m = mark(&x);
				if (ends(&x, &E, g->root->entry, g->root->end, s)) break;
				if (g->minimal) {
					for (t = s; t <= E.last && !found && !x.err; t++)
						if (bs_has(&E, t)) found = bt(&x, g->root, s, t, 0, 1);
					t--;
				} else {
					for (t = E.last; t >= s && !found && !x.err; t--)
						if (bs_has(&E, t)) found = bt(&x, g->root, s, t, 0, 1);
					t++;
				}
				if (found) { so = s; eo = t; }
				else release(&x, m);
			}
		} else {
			found = phase1(&x, &so, &eo, !want);
			if (found && want && (g->nsub || g->minimal)) {
				BS A;
				if (!(g->minimal ? ends(&x, &A, g->root->entry, g->root->end, so) : single(&x, &A, eo)))
					eo = solve(&x, g->root, so, &A);
			}
		}
	}
	free(x.dense); free(x.sparse); free(x.dense2); free(x.sparse2);
	free(x.stk); free(x.start); free(x.start2);
	release(&x, (Mark){ 0, 0 });
	if (x.err) { free(x.cap); return x.err; }
	if (!found) { free(x.cap); return REG_NOMATCH; }
	x.cap[0].rm_so = so;
	x.cap[0].rm_eo = eo;
	for (i = 0; i < nmatch; i++) {
		if (i < ncap) pm[i] = x.cap[i];
		else pm[i].rm_so = pm[i].rm_eo = -1;
	}
	free(x.cap);
	return 0;
}

int regexec(const regex_t *__RESTRICT re, const char *__RESTRICT str, size_t nmatch,
            regmatch_t *__RESTRICT pm, int eflags)
{
	const struct re_prog *g = re->__re_prog;
	regmatch_t one, *m = pm;
	size_t n, len, i, k;
	long *off, lo;
	char *u;
	int r;
	if (!g->mbcs)
		return exec(re, str, nmatch, pm, eflags);
	/* a multibyte codeset: the string as UTF-8, the offsets mapped back */
	lo = eflags & REG_STARTEND ? (long)pm[0].rm_so : 0;
	n = eflags & REG_STARTEND ? (size_t)pm[0].rm_eo : strlen(str);
	if (!(u = __re_to_u8(str, n, g->mbcs, &off, &len)))
		return REG_ESPACE;
	if (g->flags & REG_NOSUB) nmatch = 0;
	if (!nmatch) {
		m = &one;
		nmatch = 1;
	}
	for (k = 0; k < len && off[k] < lo; k++)
		;
	m[0].rm_so = (regoff_t)k;
	m[0].rm_eo = (regoff_t)len;
	r = exec(re, u, nmatch, m, eflags | REG_STARTEND);
	if (!r && m != &one)
		for (i = 0; i < nmatch; i++)
			if (m[i].rm_so >= 0) {
				m[i].rm_so = (regoff_t)off[m[i].rm_so];
				m[i].rm_eo = (regoff_t)off[m[i].rm_eo];
			}
	free(u);
	free(off);
	return r;
}
