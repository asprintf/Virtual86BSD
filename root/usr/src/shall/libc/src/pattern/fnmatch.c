#include <fnmatch.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "locale_impl.h"
#include "mb_impl.h"

/* Pattern matching notation (XCU 2.14), by characters of the locale: in
 * a UTF-8 locale a character is a multibyte sequence (a byte that does
 * not start a valid one stands for itself), else a byte. */

struct m {
	int flags;
	const struct __lcat *ct, *co;   /* LC_CTYPE, LC_COLLATE */
	int mb;
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

/* one element of a bracket expression: a character, or that of a
 * collating symbol or equivalence class of one character */
static const unsigned char *elem(const struct m *m, const unsigned char *p, unsigned *c)
{
	if (p[0] == '[' && (p[1] == '.' || p[1] == '=')) {
		int d = p[1];
		size_t l = get(m, p + 2, c);
		if (!p[2] || p[2 + l] != d || p[3 + l] != ']') return 0;
		return p + 4 + l;
	}
	if (p[0] == '\\' && !(m->flags & FNM_NOESCAPE) && p[1]) p++;
	if (!*p) return 0;
	return p + get(m, p, c);
}

/* Match the character ch against the bracket expression after '['.
 * Returns the pattern after the closing ']' and sets *ok, or null if
 * this '[' does not start a valid bracket expression. */
static const unsigned char *bracket(const struct m *m, const unsigned char *p, unsigned ch, int *ok)
{
	int neg = 0, first = 1, hit = 0;
	unsigned c, d, fch = fold(m, ch);
	if (*p == '!' || *p == '^') { neg = 1; p++; }
	for (;; first = 0) {
		if (!*p) return 0;
		if (*p == ']' && !first) { p++; break; }
		if ((m->flags & FNM_PATHNAME) && *p == '/') return 0;
		if (p[0] == '[' && p[1] == ':') {
			const unsigned char *e = p + 2;
			char name[32];
			size_t l;
			unsigned long t;
			while (*e && !(e[0] == ':' && e[1] == ']')) e++;
			if (!*e) return 0;
			l = (size_t)(e - (p + 2));
			if (l < sizeof name) {
				memcpy(name, p + 2, l);
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
			/* an equivalence class: the characters of the same first-level weights */
			if (!(p = elem(m, p, &c))) return 0;
			if (fold(m, c) == fch || (c < 0x110000 && ch < 0x110000 &&
			    (__coll_equiv(m->co, 1, c, ch) || (fch != ch && __coll_equiv(m->co, 1, c, fch)))))
				hit = 1;
			continue;
		}
		if (!(p = elem(m, p, &c))) return 0;
		if (p[0] == '-' && p[1] && p[1] != ']') {
			if (!(p = elem(m, p + 1, &d))) return 0;
			if (m->co->nrules && m->co->collseqwc && c < 0x110000 && d < 0x110000) {
				/* by the collation sequence, as glibc */
				uint32_t sc = __coll_seq(m->co, 1, c), sd = __coll_seq(m->co, 1, d), sx;
#define INR(x) ((x) < 0x110000 && (sx = __coll_seq(m->co, 1, (x))) >= sc && sx <= sd)
				if (INR(ch)) hit = 1;
				else if ((m->flags & FNM_CASEFOLD) && ch < 0x110000 &&
				         (INR(__towctrans_l(ch, 1, m->ct)) || INR(__towctrans_l(ch, 2, m->ct))))
					hit = 1;
#undef INR
			} else if (c <= ch && ch <= d) hit = 1;
			else if ((m->flags & FNM_CASEFOLD) && ch < 0x110000) {
				unsigned u = __towctrans_l(ch, 1, m->ct), l = __towctrans_l(ch, 2, m->ct);
				if ((c <= u && u <= d) || (c <= l && l <= d)) hit = 1;
			}
		} else if (fold(m, c) == fch)
			hit = 1;
	}
	*ok = hit != neg;
	return p;
}

int fnmatch(const char *pattern, const char *string, int flags)
{
	const unsigned char *p = (const unsigned char *)pattern, *s = (const unsigned char *)string;
	const unsigned char *str = s, *sp = 0, *ss = 0, *q;
	struct m m;
	unsigned pc, sc;
	size_t sl;
	int ok;
	m.flags = flags;
	m.ct = CURRENT_CTYPE;
	m.co = CURRENT_LOCALE->cat[LC_COLLATE];
	m.mb = m.ct->cs == CS_UTF8 || m.ct->cs == CS_MB;
#define LEADING(t) ((flags & FNM_PERIOD) && *(t) == '.' && \
	((t) == str || ((flags & FNM_PATHNAME) && (t)[-1] == '/')))
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
		sl = get(&m, s, &sc);
		if (*p == '?') {
			if (((flags & FNM_PATHNAME) && *s == '/') || LEADING(s)) goto fail;
			p++; s += sl;
			continue;
		}
		if (*p == '[' && (q = bracket(&m, p + 1, sc, &ok))) {
			if (!ok || ((flags & FNM_PATHNAME) && *s == '/') || LEADING(s)) goto fail;
			p = q; s += sl;
			continue;
		}
		if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1]) p++;
		p += get(&m, p, &pc);
		if (fold(&m, pc) != fold(&m, sc)) goto fail;
		s += sl;
		continue;
	fail:
		/* let the last star take one more character */
		if (!sp || !*ss || ((flags & FNM_PATHNAME) && *ss == '/')) return FNM_NOMATCH;
		ss += get(&m, ss, &sc);
		p = sp; s = ss;
	}
#undef LEADING
}
