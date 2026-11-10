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

#include <fnmatch.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "locale_impl.h"
#include "mb_impl.h"

/* Pattern matching notation (XCU 2.14), by characters of the locale: in
 * a UTF-8 locale a character is a multibyte sequence (a byte that does
 * not start a valid one stands for itself), else a byte.
 *
 * A bracket expression matches one character, or one of the locale's
 * multi-character collating elements ("ch") named by [.ch.] or of an
 * equivalence class [=x=]; ranges and non-matching lists match single
 * characters (as in regcomp()).  Where both a character and an element
 * match, both are tried.
 *
 * Without FNM_NOESCAPE a backslash escapes the next character everywhere,
 * also in the names of [:class:], [.symbol.] and [=class=] (XCU 2.14.1:
 * "regardless of whether or not the <backslash> is inside a bracket
 * expression").  With FNM_PATHNAME a slash is matched only by a slash,
 * never by a bracket expression; a bracket expression with a slash in it
 * is still one (fnmatch() takes XCU 2.14.1 and 2.14.2, not the rule of
 * 2.14.3 that makes its '[' ordinary: glob() and the shell split the
 * pattern at slashes first, pax does that rule itself). */

#define NAMEMAX 64

struct m {
	int flags;
	const struct __lcat *ct, *co;   /* LC_CTYPE, LC_COLLATE */
	int mb;
	int elems;                      /* co has multi-character elements */
	const unsigned char *str;       /* the whole string */
};

/* a collating element: one character, or several (code points) */
struct ce {
	uint32_t u[NAMEMAX];
	size_t n;
};

/* the character at s: its value (bytes >= 0x80 that are not characters
 * become 0x110000 + byte, matching only themselves); returns its length */
static size_t get(const struct m *m, const unsigned char *s, unsigned *c)
{
	wchar_t w;
	mbstate_t st = { { 0, 0 } };
	size_t l;
	if (*s < 0x80 || !m->mb) {
		if (*s < 0x80 || m->ct->cs != CS_8BIT) { *c = *s; return 1; }
		l = __mbrtowc_l(&w, (const char *)s, 1, &st, m->ct);
		*c = l == 1 ? (unsigned)w : 0x110000u + *s;
		return 1;
	}
	l = __mbrtowc_l(&w, (const char *)s, 4, &st, m->ct);
	if (l == (size_t)-1 || l == (size_t)-2 || !l) { *c = 0x110000u + *s; return 1; }
	*c = (unsigned)w;
	return l;
}

static unsigned fold(const struct m *m, unsigned c)
{
	return (m->flags & FNM_CASEFOLD) && c < 0x110000 ? __towctrans_l(c, 2, m->ct) : c;
}

static int same(const struct m *m, const struct ce *a, const struct ce *b)
{
	size_t i;
	if (a->n != b->n) return 0;
	for (i = 0; i < a->n; i++)
		if (fold(m, a->u[i]) != fold(m, b->u[i])) return 0;
	return 1;
}

/* one element of a bracket expression: a character, or a collating
 * symbol or equivalence class (of one character, or of a collating
 * element of the locale); null if it is not valid */
static const unsigned char *elem(const struct m *m, const unsigned char *p, struct ce *x)
{
	unsigned c;
	x->n = 0;
	if (p[0] == '[' && (p[1] == '.' || p[1] == '=')) {
		int d = p[1];
		const unsigned char *q = p + 2;
		do {
			if (!*q || x->n == NAMEMAX) return 0;
			/* escapes work here too (XCU 2.14.1): [[.\..]] is [[...]] */
			if (q[0] == '\\' && !(m->flags & FNM_NOESCAPE) && q[1]) q++;
			q += get(m, q, &c);
			x->u[x->n++] = c;
		} while (!(q[0] == d && q[1] == ']'));
		if (x->n > 1 && !__coll_valids(m->co, 1, x->u, x->n)) return 0;
		return q + 2;
	}
	if (p[0] == '\\' && !(m->flags & FNM_NOESCAPE) && p[1]) p++;
	if (!*p) return 0;
	p += get(m, p, &c);
	x->u[x->n++] = c;
	return p;
}

/* the collation sequence of a character in a range: also of its case
 * variants with FNM_CASEFOLD */
static int inrange(const struct m *m, unsigned ch, uint32_t sc, uint32_t sd)
{
	uint32_t sx;
#define INR(x) ((x) < 0x110000 && (sx = __coll_seq(m->co, 1, (x))) >= sc && sx <= sd)
	if (INR(ch)) return 1;
	return (m->flags & FNM_CASEFOLD) && ch < 0x110000 &&
	       (INR(__towctrans_l(ch, 1, m->ct)) || INR(__towctrans_l(ch, 2, m->ct)));
#undef INR
}

/* Match the collating element x (a character, or several) against the
 * bracket expression after '['.  Returns the pattern after the closing
 * ']' and sets *ok (and *want: whether elements of several characters
 * can match), or null if this '[' does not start a valid bracket
 * expression. */
static const unsigned char *bracket(const struct m *m, const unsigned char *p, const struct ce *x,
                                    int *ok, int *want)
{
	int neg = 0, first = 1, hit = 0, multi = 0;
	unsigned ch = x->u[0], fch = fold(m, ch);
	struct ce c, d;
	if (*p == '!' || *p == '^') { neg = 1; p++; }
	for (;; first = 0) {
		if (!*p) return 0;
		if (*p == ']' && !first) { p++; break; }
		if (p[0] == '[' && p[1] == ':') {
			/* the class name, without its escapes ([[:al\num:]] is
			 * [[:alnum:]]: XCU 2.14.1 escapes inside bracket
			 * expressions too) */
			const unsigned char *e = p + 2;
			char name[32];
			size_t l = 0;
			unsigned long t;
			while (*e && !(e[0] == ':' && e[1] == ']')) {
				if (e[0] == '\\' && !(m->flags & FNM_NOESCAPE) && e[1]) e++;
				if (l < sizeof name) name[l] = (char)*e;
				l++, e++;
			}
			if (!*e) return 0;
			if (l < sizeof name && x->n == 1) {
				name[l] = 0;
				t = __wctype_l(name, m->ct);
				if (t && ch < 0x110000) {
					if (__iswctype_l(ch, t, m->ct)) hit = 1;
					else if ((m->flags & FNM_CASEFOLD) && (!strcmp(name, "upper") || !strcmp(name, "lower")) &&
					         (__iswctype_l(ch, 1 + W_UPPER, m->ct) || __iswctype_l(ch, 1 + W_LOWER, m->ct)))
						hit = 1;
				}
			}
			p = e + 2;
			continue;
		}
		if (p[0] == '[' && p[1] == '=') {
			/* an equivalence class: the characters and elements of the
			 * same first-level weights */
			if (!(p = elem(m, p, &c))) return 0;
			multi = 1;
			if (same(m, &c, x)) hit = 1;
			else if (c.n == 1 && x->n == 1) {
				if (c.u[0] < 0x110000 && ch < 0x110000 &&
				    (__coll_equiv(m->co, 1, c.u[0], ch) || (fch != ch && __coll_equiv(m->co, 1, c.u[0], fch))))
					hit = 1;
			} else if (__coll_equivs(m->co, 1, c.u, c.n, x->u, x->n))
				hit = 1;
			continue;
		}
		if (!(p = elem(m, p, &c))) return 0;
		if (p[0] == '-' && p[1] && p[1] != ']') {
			if (!(p = elem(m, p + 1, &d))) return 0;
			if (m->co->nrules && m->co->collseqwc && c.u[0] < 0x110000 && d.u[0] < 0x110000) {
				/* by the collation sequence, as glibc */
				uint32_t sc = __coll_seqs(m->co, 1, c.u, c.n);
				uint32_t sd = __coll_seqs(m->co, 1, d.u, d.n);
				if (sc == ~0u || sd == ~0u) continue;
				if (x->n == 1 && inrange(m, ch, sc, sd))
					hit = 1;
			} else if (x->n == 1 && c.n == 1 && d.n == 1) {
				if (c.u[0] <= ch && ch <= d.u[0]) hit = 1;
				else if ((m->flags & FNM_CASEFOLD) && ch < 0x110000) {
					unsigned u = __towctrans_l(ch, 1, m->ct), l = __towctrans_l(ch, 2, m->ct);
					if ((c.u[0] <= u && u <= d.u[0]) || (c.u[0] <= l && l <= d.u[0])) hit = 1;
				}
			}
		} else {
			if (c.n > 1) multi = 1;
			if (same(m, &c, x)) hit = 1;
		}
	}
	/* elements of several characters: only as named or of a class */
	*ok = x->n > 1 ? hit && !neg : hit != neg;
	*want = multi && !neg;
	return p;
}

static int match(const struct m *m, const unsigned char *p, const unsigned char *s)
{
	const unsigned char *sp = 0, *ss = 0, *q;
	struct ce x;
	unsigned pc, sc;
	size_t sl;
	int ok, want, flags = m->flags;
#define LEADING(t) ((flags & FNM_PERIOD) && *(t) == '.' && \
	((t) == m->str || ((flags & FNM_PATHNAME) && (t)[-1] == '/')))
	for (;;) {
		if (*p == '*') {
			while (*p == '*') p++;
			/* a leading period is matched only by a period that comes
			 * first in the pattern (or after a slash) */
			if (LEADING(s)) return FNM_NOMATCH;
			sp = p; ss = s;
			continue;
		}
		if (!*s) {
			if (!*p) return 0;
			goto fail;
		}
		if (!*p) goto fail;
		sl = get(m, s, &sc);
		if (*p == '?') {
			if (((flags & FNM_PATHNAME) && *s == '/') || LEADING(s)) goto fail;
			p++; s += sl;
			continue;
		}
		x.u[0] = sc;
		x.n = 1;
		if (*p == '[' && (q = bracket(m, p + 1, &x, &ok, &want))) {
			if (((flags & FNM_PATHNAME) && *s == '/') || LEADING(s)) goto fail;
			if (m->elems && want) {
				/* the collating elements of the locale that start here */
				struct __coll_me e;
				uint32_t u[256];
				size_t pos = 0, n, k;
				const unsigned char *t;
				int eok;
				while (__coll_me_next(m->co, &pos, &e)) {
					if ((n = __coll_me_units(&e, 1, u)) < 2 || n > NAMEMAX) continue;
					for (k = 0, t = s; k < n && *t; k++) {
						unsigned c;
						t += get(m, t, &c);
						if (fold(m, c) != fold(m, u[k]) || ((flags & FNM_PATHNAME) && c == '/')) break;
					}
					if (k < n) continue;
					memcpy(x.u, u, n * sizeof *u);
					x.n = n;
					if (bracket(m, p + 1, &x, &eok, &want) && eok && !match(m, q, t)) return 0;
				}
			}
			if (!ok) goto fail;
			p = q; s += sl;
			continue;
		}
		if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1]) p++;
		p += get(m, p, &pc);
		if (fold(m, pc) != fold(m, sc)) goto fail;
		s += sl;
		continue;
	fail:
		/* let the last star take one more character */
		if (!sp || !*ss || ((flags & FNM_PATHNAME) && *ss == '/')) return FNM_NOMATCH;
		ss += get(m, ss, &sc);
		p = sp; s = ss;
	}
#undef LEADING
}

int fnmatch(const char *pattern, const char *string, int flags)
{
	struct m m;
	struct __coll_me e;
	size_t pos = 0;
	m.flags = flags;
	m.ct = CURRENT_CTYPE;
	m.co = CURRENT_LOCALE->cat[LC_COLLATE];
	m.mb = m.ct->cs == CS_UTF8 || m.ct->cs == CS_MB;
	m.elems = __coll_me_next(m.co, &pos, &e);
	m.str = (const unsigned char *)string;
	return match(&m, (const unsigned char *)pattern, (const unsigned char *)string);
}
