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

/* Collation by the tables of a locale's LC_COLLATE file (glibc's format,
 * as the localedef of the POSIX utilities and glibc's write them).
 *
 * A string is cut into collating elements: the multibyte table (or, for
 * wide strings, the 3-level table) gives for the first byte (character)
 * either the element's index or a list of the longer sequences that
 * start with it (contractions such as "ch", and ranges). An index holds
 * the rule set in its top byte and, in the low 24 bits, the position of
 * its weights: for each level a length and that many weights.
 *
 * Level by level, the weights of the elements make a string of units:
 * in order, except that runs of elements whose rule is not "forward" at
 * that level are taken backward; with "position" every element with
 * weights is preceded by the number of ignored ones before it, plus 1.
 * strxfrm writes the levels separated by 1 and ended by 0 (and drops a
 * last empty level's separator), as glibc does.  strcoll follows glibc's
 * algorithm (below), which can order strings other than their
 * transforms do.  Without rules (C, POSIX, C.UTF-8):
 * code point order. */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "locale_impl.h"

#define FWD 1
#define POS 4

struct elt {
	int32_t idx;
	unsigned char rule;
};

struct eltbuf {
	struct elt *e;
	size_t n, cap;
	struct elt small[64];
};

struct ubuf {
	int32_t *u;
	size_t n, cap;
	int32_t small[256];
};

static int grow(void **p, size_t *cap, size_t size, void *small)
{
	size_t nc = *cap * 2;
	void *q = *p == small ? malloc(nc * size) : realloc(*p, nc * size);
	if (!q) return -1;
	if (*p == small) memcpy(q, small, *cap * size);
	*p = q;
	*cap = nc;
	return 0;
}

static int put_elt(struct eltbuf *b, int32_t t)
{
	if (b->n == b->cap && grow((void **)&b->e, &b->cap, sizeof *b->e, b->small)) return -1;
	b->e[b->n].idx = t & 0xffffff;
	b->e[b->n].rule = (unsigned char)((uint32_t)t >> 24);
	b->n++;
	return 0;
}

static int put_unit(struct ubuf *b, int32_t v)
{
	if (b->n == b->cap && grow((void **)&b->u, &b->cap, sizeof *b->u, b->small)) return -1;
	b->u[b->n++] = v;
	return 0;
}

static void init_e(struct eltbuf *b) { b->e = b->small; b->n = 0; b->cap = 64; }
static void init_u(struct ubuf *b) { b->u = b->small; b->n = 0; b->cap = 256; }
static void free_e(struct eltbuf *b) { if (b->e != b->small) free(b->e); }
static void free_u(struct ubuf *b) { if (b->u != b->small) free(b->u); }

static int32_t rd32(const unsigned char *p) { int32_t v; memcpy(&v, p, 4); return v; }

/* the element at *sp of a multibyte string; advances *sp */
static int32_t find_mb(const struct __lcat *c, const unsigned char **sp)
{
	const unsigned char *s = *sp, *cp, *end = c->extramb + c->nextramb;
	int32_t i = c->tablemb[*s++];
	size_t nhere, cnt, offset;
	*sp = s;
	if (i >= 0) return i;
	if ((size_t)-(int64_t)i >= c->nextramb) return 0;
	cp = c->extramb - (int64_t)i;
	while (cp + 5 <= end) {
		i = rd32(cp);
		nhere = cp[4];
		cp += 5;
		if (i >= 0) {
			if (cp + nhere > end) break;
			for (cnt = 0; cnt < nhere && cp[cnt] == s[cnt]; cnt++)
				;
			if (cnt == nhere) {
				*sp = s + nhere;
				return i;
			}
			cp += nhere;
			if ((1 + nhere) % 4) cp += 4 - (1 + nhere) % 4;
			continue;
		}
		/* a range of sequences */
		if (cp + 2 * nhere > end) break;
		for (cnt = 0; cnt < nhere && cp[cnt] == s[cnt]; cnt++)
			;
		offset = 0;
		if (cnt != nhere) {
			if (cp[cnt] > s[cnt]) goto next;
			for (cnt = 0; cnt < nhere && cp[nhere + cnt] == s[cnt]; cnt++)
				;
			if (cnt != nhere && cp[nhere + cnt] < s[cnt]) goto next;
			for (cnt = 0; cp[cnt] == s[cnt]; cnt++)
				;
			do offset = (offset << 8) + (size_t)((int)s[cnt] - (int)cp[cnt]);
			while (++cnt < nhere);
		}
		offset += (size_t)-(int64_t)i;
		*sp = s + nhere;
		return offset < c->nindirectmb ? c->indirectmb[offset] : 0;
	next:
		cp += 2 * nhere;
		if ((1 + 2 * nhere) % 4) cp += 4 - (1 + 2 * nhere) % 4;
	}
	return 0;
}

/* the element at *sp of a wide string */
static int32_t find_wc(const struct __lcat *c, const wchar_t **sp)
{
	const wchar_t *s = *sp;
	const int32_t *cp, *end = c->extrawc + c->nextrawc;
	int32_t i = __tab_map(c->tablewc, (uint32_t)*s++);
	size_t nhere, cnt;
	*sp = s;
	if (i >= 0) return i;
	if ((size_t)-(int64_t)i >= c->nextrawc) return 0;
	cp = c->extrawc - (int64_t)i;
	while (cp + 2 <= end) {
		i = cp[0];
		nhere = (size_t)cp[1];
		cp += 2;
		if (i >= 0) {
			if ((size_t)(end - cp) < nhere) break;
			for (cnt = 0; cnt < nhere && cp[cnt] == s[cnt]; cnt++)
				;
			if (cnt == nhere) {
				*sp = s + nhere;
				return i;
			}
			cp += nhere;
			continue;
		}
		if ((size_t)(end - cp) < 2 * nhere || !nhere) break;
		for (cnt = 0; cnt < nhere - 1 && cp[cnt] == s[cnt]; cnt++)
			;
		if (cnt < nhere - 1 || cp[nhere - 1] > s[nhere - 1] || cp[2 * nhere - 1] < s[nhere - 1]) {
			cp += 2 * nhere;
			continue;
		}
		{
			size_t off = (size_t)-(int64_t)i + (size_t)(s[nhere - 1] - cp[nhere - 1]);
			*sp = s + nhere;
			return off < c->nindirectwc ? c->indirectwc[off] : 0;
		}
	}
	return 0;
}

static int elements(const struct __lcat *c, const void *str, int wide, struct eltbuf *b)
{
	if (wide) {
		const wchar_t *s = str;
		while (*s)
			if (put_elt(b, find_wc(c, &s))) return -1;
	} else {
		const unsigned char *s = str;
		while (*s)
			if (put_elt(b, find_mb(c, &s))) return -1;
	}
	return 0;
}

static int rule(const struct __lcat *c, const struct elt *e, int p)
{
	size_t k = (size_t)e->rule * (size_t)c->nrules + (size_t)p;
	return k < c->nrulesets ? c->rulesets[k] : FWD;
}

/* the units of element e at level p */
static int emit(const struct __lcat *c, int wide, const struct elt *e, int p, int position,
                int32_t *val, struct ubuf *u)
{
	size_t at = (size_t)e->idx, len, end = wide ? c->nweightwc : c->nweightmb, i;
	int lv;
#define W(k) (wide ? c->weightwc[k] : (int32_t)c->weightmb[k])
	for (lv = 0;; lv++) {
		if (at >= end) return 0;
		len = (size_t)W(at);
		if (lv == p) break;
		at += 1 + len;
	}
	if (len > end - at - 1) return 0;
	if (position) {
		if (!len) {
			++*val;
			return 0;
		}
		if (put_unit(u, wide ? *val : (int32_t)(unsigned char)*val)) return -1;
		*val = 1;
	}
	for (i = 1; i <= len; i++)
		if (put_unit(u, W(at + i))) return -1;
	return 0;
#undef W
}

/* the units of level p */
static int units(const struct __lcat *c, int wide, const struct eltbuf *b, int p, struct ubuf *u)
{
	const struct elt *e = b->e;
	size_t n = b->n, i, j, bstart = (size_t)-1;
	int position = n && (rule(c, &e[0], p) & POS);
	int32_t val = 1;
	u->n = 0;
	for (i = 0; i <= n; i++) {
		if (i < n && !(rule(c, &e[i], p) & FWD)) {
			if (bstart == (size_t)-1) bstart = i;
			continue;
		}
		if (bstart != (size_t)-1) {
			for (j = i; j > bstart;)
				if (emit(c, wide, &e[--j], p, position, &val, u)) return -1;
			bstart = (size_t)-1;
		}
		if (i < n && emit(c, wide, &e[i], p, position, &val, u)) return -1;
	}
	return 0;
}

/* strcoll: glibc's algorithm (string/strcoll_l.c), which can order two
 * strings other than their transforms do: "position" at a level is that
 * of the rule of the first element of the first string (none at the
 * first level); backward runs are taken as glibc takes them. */
#define BWD 2

struct seq {
	int len;
	size_t val, idxmax, idxcnt, backw, backw_stop;
	const void *us, *back_us;
	unsigned char rule;
	int32_t idx, save_idx;
};

static inline int32_t find_any(const struct __lcat *c, int wide, const void **us)
{
	int32_t t;
	if (wide) {
		const wchar_t *q = *us;
		t = find_wc(c, &q);
		*us = q;
	} else {
		const unsigned char *q = *us;
		t = find_mb(c, &q);
		*us = q;
	}
	return t;
}

#define WT(k) (wide ? c->weightwc[k] : (int32_t)c->weightmb[k])
#define AT_END(p) (wide ? !*(const wchar_t *)(p) : !*(const unsigned char *)(p))

static inline void next_seq(const struct __lcat *c, int wide, struct seq *q, int pass)
{
	size_t val = 0, backw_stop = q->backw_stop, backw = q->backw;
	size_t idxcnt = q->idxcnt, idxmax = q->idxmax;
	int len = q->len, i;
	int32_t idx = q->idx;
	const void *us = q->us;
	size_t nr = (size_t)c->nrules;
	while (len == 0) {
		++val;
		if (backw_stop != ~(size_t)0) {
			if (backw == backw_stop) {
				if (idxcnt < idxmax) {
					idx = q->save_idx;
					backw_stop = ~(size_t)0;
				} else {
					idx = 0;
					break;
				}
			} else {
				size_t k = backw_stop;
				us = q->back_us;
				while (k < backw) {
					idx = find_any(c, wide, &us) & 0xffffff;
					k++;
				}
				--backw;
				us = q->us;
			}
		} else {
			int32_t prev = idx;
			backw_stop = idxmax;
			while (!AT_END(us)) {
				int32_t t = find_any(c, wide, &us);
				unsigned char r = (unsigned char)((uint32_t)t >> 24);
				size_t k = (size_t)r * nr + (size_t)pass;
				prev = idx;
				idx = t & 0xffffff;
				idxcnt = idxmax++;
				if (idxcnt == 0) q->rule = r;
				if (!(k < c->nrulesets && (c->rulesets[k] & BWD))) break;
				++idxcnt;
			}
			if (backw_stop >= idxcnt) {
				if (idxcnt == idxmax || backw_stop > idxcnt) break;
				backw_stop = ~(size_t)0;
			} else {
				q->back_us = q->us;
				q->us = us;
				backw = idxcnt;
				if (idxmax > idxcnt) {
					backw--;
					q->save_idx = idx;
					idx = prev;
				}
				if (backw > backw_stop) backw--;
			}
		}
		len = (int)WT(idx);
		idx++;
		for (i = 0; i < pass; i++) {
			idx += len;
			len = (int)WT(idx);
			idx++;
		}
	}
	q->val = val;
	q->len = len;
	q->backw_stop = backw_stop;
	q->backw = backw;
	q->idxcnt = idxcnt;
	q->idxmax = idxmax;
	q->us = us;
	q->idx = idx;
}

static inline int do_compare(const struct __lcat *c, int wide, struct seq *a, struct seq *b, int position)
{
	int la = a->len, lb = b->len, r = 0;
	int32_t ia = a->idx, ib = b->idx;
	if (position && a->val != b->val) {
		r = a->val > b->val ? 1 : -1;
		goto out;
	}
	do {
		if (WT(ia) != WT(ib)) {
			r = (int)(WT(ia) - WT(ib));   /* (as glibc: the difference) */
			goto out;
		}
		ia++; ib++; la--; lb--;
	} while (la > 0 && lb > 0);
	if (position && la != lb) r = la - lb;
out:
	a->len = la; b->len = lb;
	a->idx = ia; b->idx = ib;
	return r;
}

static inline int coll_cmp(const struct __lcat *c, const void *s1, const void *s2, int wide)
{
	struct seq a, b;
	int pass, r = 0;
	unsigned char rule = 0;
	if (!c->nrules)
		return wide ? wcscmp(s1, s2) : strcmp(s1, s2);
	if (AT_END(s1) || AT_END(s2)) return !AT_END(s1) - !AT_END(s2);
	a.len = 0; a.idxmax = 0; a.rule = 0; a.save_idx = 0; a.back_us = 0;
	b.len = 0; b.idxmax = 0; b.rule = 0; b.save_idx = 0; b.back_us = 0;
	for (pass = 0; pass < c->nrules; pass++) {
		size_t k = (size_t)rule * (size_t)c->nrules + (size_t)pass;
		int position = k < c->nrulesets && (c->rulesets[k] & POS);
		a.idxcnt = 0; a.idx = 0; a.backw_stop = ~(size_t)0; a.backw = ~(size_t)0;
		b.idxcnt = 0; b.idx = 0; b.backw_stop = ~(size_t)0; b.backw = ~(size_t)0;
		a.us = s1;
		b.us = s2;
		for (;;) {
			next_seq(c, wide, &a, pass);
			next_seq(c, wide, &b, pass);
			if (a.len == 0 || b.len == 0) {
				if (a.len == b.len) {
					if (pass == 0 && !(wide ? wcscmp(s1, s2) : strcmp(s1, s2))) return r;
					break;
				}
				return a.len == 0 ? -1 : 1;
			}
			if ((r = do_compare(c, wide, &a, &b, position))) return r;
		}
		rule = a.rule;
	}
	return r;
}

int __coll_cmp(const struct __lcat *c, const void *s1, const void *s2, int wide)
{
	/* (two instances, for bytes and for wide characters) */
	return wide ? coll_cmp(c, s1, s2, 1) : coll_cmp(c, s1, s2, 0);
}
#undef WT
#undef AT_END

/* the transformed string (bytes or wide characters) into d[0..n);
 * returns its length */
size_t __coll_xfrm(const struct __lcat *c, void *d, const void *s, size_t n, int wide)
{
	struct eltbuf e;
	struct ubuf u;
	size_t need = 0, last = 0, i;
	int p;
	unsigned char *db = d;
	wchar_t *dw = d;
#define PUT(v) do { if (need < n) { if (wide) dw[need] = (wchar_t)(v); else db[need] = (unsigned char)(v); } need++; } while (0)
	if (!c->nrules) {
		size_t l = wide ? wcslen(s) : strlen(s);
		if (l < n) {
			if (wide) wmemcpy(d, s, l + 1);
			else memcpy(d, s, l + 1);
		}
		return l;
	}
	/* the empty string: empty (glibc) */
	if (wide ? !*(const wchar_t *)s : !*(const char *)s) {
		if (n) {
			if (wide) dw[0] = 0;
			else db[0] = 0;
		}
		return 0;
	}
	init_e(&e);
	init_u(&u);
	if (elements(c, s, wide, &e)) goto oom;
	for (p = 0; p < c->nrules; p++) {
		if (units(c, wide, &e, p, &u)) goto oom;
		last = need;
		for (i = 0; i < u.n; i++) PUT(u.u[i]);
		PUT(p + 1 < c->nrules ? 1 : 0);
	}
	/* a last level with nothing: no separator before the end (glibc) */
	if (need > 2 && need == last + 1) {
		need--;
		if (need <= n) {
			if (wide) dw[need - 1] = 0;
			else db[need - 1] = 0;
		}
	}
	free_e(&e);
	free_u(&u);
	return need - 1;
oom:
	free_e(&e);
	free_u(&u);
	return (size_t)-1;
#undef PUT
}

/* the first-level weights of the collating element x[0..n) (wide: code
 * points; else bytes), with its rule; 0 if it is not one element */
static int primary(const struct __lcat *c, int wide, const uint32_t *x, size_t n,
                   int32_t *t, size_t *at, size_t *len)
{
	size_t end = wide ? c->nweightwc : c->nweightmb, i;
	if (!n || n > 255) return 0;
	if (wide) {
		wchar_t s[256];
		const wchar_t *p = s;
		for (i = 0; i < n; i++) s[i] = (wchar_t)x[i];
		s[n] = 0;
		*t = find_wc(c, &p);
		if (p != s + n) return 0;
	} else {
		unsigned char s[256];
		const unsigned char *p = s;
		for (i = 0; i < n; i++) s[i] = (unsigned char)x[i];
		s[n] = 0;
		*t = find_mb(c, &p);
		if (p != s + n) return 0;
	}
	if (!*t) return 0;
	*at = (size_t)(*t & 0xffffff);
	if (*at >= end) return 0;
	*len = wide ? (size_t)c->weightwc[*at] : c->weightmb[*at];
	return *len <= end - *at - 1;
}

/* x and y are of one equivalence class: the same first-level weights
 * (as glibc's regex decides it); without collation rules, x == y */
int __coll_equivs(const struct __lcat *c, int wide, const uint32_t *x, size_t nx,
                  const uint32_t *y, size_t ny)
{
	int32_t tx, ty;
	size_t ax, lx, ay, ly, i;
	if (nx == ny && !memcmp(x, y, nx * sizeof *x)) return 1;
	if (!c->nrules || !primary(c, wide, x, nx, &tx, &ax, &lx) || !primary(c, wide, y, ny, &ty, &ay, &ly))
		return 0;
	if (lx != ly || (tx >> 24) != (ty >> 24)) return 0;
	for (i = 1; i <= lx; i++)
		if (wide ? c->weightwc[ax + i] != c->weightwc[ay + i] : c->weightmb[ax + i] != c->weightmb[ay + i])
			return 0;
	return 1;
}

int __coll_equiv(const struct __lcat *c, int wide, uint32_t x, uint32_t y)
{
	return __coll_equivs(c, wide, &x, 1, &y, 1);
}

/* a character (wide: a code point; else a byte) with a collating element
 * of its own (glibc's regcomp refuses an equivalence class of another) */
int __coll_valid(const struct __lcat *c, int wide, uint32_t x)
{
	int32_t t;
	size_t at, len;
	return !c->nrules || primary(c, wide, &x, 1, &t, &at, &len);
}

/* ---- multi-character collating elements ---- */

int __coll_me_next(const struct __lcat *c, size_t *pos, struct __coll_me *e)
{
	const unsigned char *x = c->symbextra;
	size_t i = *pos, n = c->nsymbextra;
	if (!c->nrules || !x) return 0;
	/* [len][name] [len][bytes] pad to 4, seqmb, nwcs, wcs, seqwc */
	if (i >= n || (i += 1 + (size_t)x[i]) >= n) return 0;
	e->nmbs = x[i];
	e->mbs = x + i + 1;
	i = (i + 1 + e->nmbs + 3) & ~(size_t)3;
	if (i + 8 > n) return 0;
	e->seqmb = (uint32_t)rd32(x + i);
	e->nwcs = (uint32_t)rd32(x + i + 4);
	if (e->nwcs > 255 || i + 12 + 4 * e->nwcs > n) return 0;
	e->wcs = x + i + 8;
	e->seqwc = (uint32_t)rd32(x + i + 8 + 4 * e->nwcs);
	*pos = i + 12 + 4 * e->nwcs;
	return 1;
}

size_t __coll_me_units(const struct __coll_me *e, int wide, uint32_t *u)
{
	size_t i;
	if (wide)
		for (i = 0; i < e->nwcs; i++) u[i] = (uint32_t)rd32(e->wcs + 4 * i);
	else
		for (i = 0; i < e->nmbs; i++) u[i] = e->mbs[i];
	return wide ? e->nwcs : e->nmbs;
}

/* the multi-character element x[0..n), or 0 */
static int me_find(const struct __lcat *c, int wide, const uint32_t *x, size_t n, struct __coll_me *e)
{
	uint32_t u[256];
	size_t pos = 0;
	while (__coll_me_next(c, &pos, e))
		if (__coll_me_units(e, wide, u) == n && !memcmp(u, x, n * sizeof *x))
			return 1;
	return 0;
}

int __coll_valids(const struct __lcat *c, int wide, const uint32_t *x, size_t n)
{
	struct __coll_me e;
	if (n == 1) return __coll_valid(c, wide, *x);
	return n && me_find(c, wide, x, n, &e);
}

uint32_t __coll_seqs(const struct __lcat *c, int wide, const uint32_t *x, size_t n)
{
	struct __coll_me e;
	if (n == 1) return __coll_seq(c, wide, *x);
	return n && me_find(c, wide, x, n, &e) ? e.seqwc : ~0u;
}

/* The collation sequence of a character (wide: a code point; else a
 * byte): the order of bracket ranges in regex and fnmatch, as glibc has
 * them in locales with collation rules. ~0 if it has none. */
uint32_t __coll_seq(const struct __lcat *c, int wide, uint32_t x)
{
	const uint32_t *t = c->collseqwc;
	uint32_t i1, l1, l2;
	if (!wide) return c->collseqmb ? c->collseqmb[x & 255] : x;
	if (!t) return x;
	i1 = x >> t[0];
	if (i1 >= t[1] || !(l1 = t[5 + i1])) return ~0u;
	if (!(l2 = *(const uint32_t *)(const void *)((const char *)t + l1 + 4 * ((x >> t[2]) & t[3])))) return ~0u;
	return ((const uint32_t *)(const void *)((const char *)t + l2))[x & t[4]];
}

/* The runs of code points whose collation sequence is in [sc, sd], in
 * increasing order: f(arg, first, last) for each (stops at f's nonzero
 * result, which it returns).  Walks the 3-level table block by block. */
struct runs {
	int (*f)(void *, uint32_t, uint32_t);
	void *arg;
	uint32_t sc, sd, start;
	int in;
};

static int feed(struct runs *r, uint32_t a, uint32_t b, uint32_t seq)
{
	int in = seq >= r->sc && seq <= r->sd, k;
	if (in && !r->in) { r->start = a; r->in = 1; }
	else if (!in && r->in) {
		r->in = 0;
		if ((k = r->f(r->arg, r->start, a - 1))) return k;
	}
	(void)b;
	return 0;
}

int __coll_seq_runs(const struct __lcat *c, uint32_t sc, uint32_t sd,
                    int (*f)(void *, uint32_t, uint32_t), void *arg)
{
	const uint32_t *t = c->collseqwc;
	struct runs r;
	uint32_t i1, i2, i3, x = 0, n1, n2, n3;
	int k;
	r.f = f; r.arg = arg; r.sc = sc; r.sd = sd; r.in = 0; r.start = 0;
	if (!t) return 0;
	n2 = t[3] + 1;
	n3 = t[4] + 1;
	n1 = t[1];
	for (i1 = 0; i1 < n1 && x < 0x110000; i1++) {
		uint32_t l1 = t[5 + i1];
		if (!l1) {
			if ((k = feed(&r, x, x + (1u << t[0]) - 1, ~0u))) return k;
			x += 1u << t[0];
			continue;
		}
		for (i2 = 0; i2 < n2; i2++) {
			uint32_t l2 = *(const uint32_t *)(const void *)((const char *)t + l1 + 4 * i2);
			if (!l2) {
				if ((k = feed(&r, x, x + (1u << t[2]) - 1, ~0u))) return k;
				x += 1u << t[2];
				continue;
			}
			for (i3 = 0; i3 < n3 && x < 0x110000; i3++, x++)
				if ((k = feed(&r, x, x, ((const uint32_t *)(const void *)((const char *)t + l2))[i3]))) return k;
		}
	}
	if (x < 0x110000 && (k = feed(&r, x, 0x10ffff, ~0u))) return k;
	if (r.in) return f(arg, r.start, 0x10ffff);
	return 0;
}
