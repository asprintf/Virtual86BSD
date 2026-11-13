/*
 * Copyright (c) 2026, asprintf
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

/* Simple regular expressions of SUSv2: <regexp.h> and re_comp */
/* Program in expbuf: [0] 'R', [1] flags (1 ^, 2 newline rules), [2] number
 * of \( groups, [3..4] length, then the ops.  Backtracking stops at locs. */
#define _DEFAULT_SOURCE
#include <regex.h>
#include <stdlib.h>
#include <string.h>

enum { OCHR = 1, ODOT, OCCL, OBACK, ODOL, OEOF, OBRA, OKET };
#define STAR 0x40
#define RNGE 0x80
#define RINF 0x20
#define OP(c) ((c) & 0x1f)

#define F_CIRC 1
#define F_NL   2
#define HDR    5
#define NBRA   9

int __sre_compile(const char *p, size_t n, unsigned char *buf, size_t size, int flags)
{
	unsigned char *ep = buf + HDR, *end = buf + size, *lastep = 0;
	int stack[NBRA], depth = 0, nbra = 0;
	size_t i = 0;
	if (size < HDR + 1)
		return -50;
	buf[1] = (unsigned char)(flags & F_NL);
	if (n && p[0] == '^') {
		buf[1] |= F_CIRC;
		i++;
	}
#define NEED(k) do { if (end - ep < (k)) return -50; } while (0)
	while (i < n) {
		int c = (unsigned char)p[i++];
		switch (c) {
		case '.':
			NEED(1);
			lastep = ep;
			*ep++ = ODOT;
			continue;
		case '*':
			if (!lastep || (*lastep & RNGE))
				goto literal;
			*lastep |= STAR;
			continue;
		case '$':
			if (i != n)
				goto literal;
			NEED(1);
			*ep++ = ODOL;
			lastep = 0;
			continue;
		case '[': {
			size_t j = i, k;
			char *txt;
			regex_t re;
			int neg = 0, b;
			if (j < n && p[j] == '^')
				neg = 1, j++;
			if (j < n && p[j] == ']')
				j++;
			while (j < n && p[j] != ']') {
				if (p[j] == '[' && j + 1 < n && (p[j + 1] == ':' || p[j + 1] == '=' || p[j + 1] == '.')) {
					for (k = j + 2; k + 1 < n && !(p[k] == p[j + 1] && p[k + 1] == ']'); k++)
						;
					if (k + 1 >= n)
						return -49;
					j = k + 2;
				} else
					j++;
			}
			if (j >= n)
				return -49;
			NEED(33);
			if (!(txt = malloc(j - i + 3)))
				return -50;
			txt[0] = '[';
			memcpy(txt + 1, p + i, j - i + 1);
			txt[j - i + 2] = 0;
			b = regcomp(&re, txt, REG_NOSUB);
			free(txt);
			if (b)
				return -49;
			lastep = ep;
			*ep++ = OCCL;
			memset(ep, 0, 32);
			for (b = 1; b < 256; b++) {
				char s[2] = { (char)b, 0 };
				if (neg && b == '\n' && (flags & F_NL))
					continue;
				if (!regexec(&re, s, 0, 0, 0))
					ep[b >> 3] |= (unsigned char)(1 << (b & 7));
			}
			regfree(&re);
			ep += 32;
			i = j + 1;
			continue;
		}
		case '\\':
			if (i >= n)
				return -36;
			c = (unsigned char)p[i++];
			if (c == '(') {
				if (nbra >= NBRA)
					return -43;
				NEED(2);
				stack[depth++] = nbra;
				*ep++ = OBRA;
				*ep++ = (unsigned char)nbra++;
				lastep = 0;
				continue;
			}
			if (c == ')') {
				if (!depth)
					return -42;
				NEED(2);
				*ep++ = OKET;
				*ep++ = (unsigned char)stack[--depth];
				lastep = 0;
				continue;
			}
			if (c == '{' && lastep && !(*lastep & (STAR | RNGE))) {
				long v[3];
				int nv = 0;
				for (;;) {
					long x = 0;
					if (i >= n || p[i] < '0' || p[i] > '9') {
						if (nv == 1 && i < n && p[i] == '\\')
							break;
						return -16;
					}
					while (i < n && p[i] >= '0' && p[i] <= '9') {
						x = x * 10 + (p[i++] - '0');
						if (x > 255)
							return -11;
					}
					if (nv == 2)
						return -44;
					v[nv++] = x;
					if (i < n && p[i] == ',') {
						if (nv == 2)
							return -44;
						i++;
						if (i < n && p[i] == '\\') {
							v[nv++] = -1;
							break;
						}
						continue;
					}
					break;
				}
				if (i + 1 >= n || p[i] != '\\' || p[i + 1] != '}')
					return -45;
				i += 2;
				if (nv == 1)
					v[1] = v[0];
				if (v[1] >= 0 && v[0] > v[1])
					return -46;
				NEED(2);
				*lastep |= RNGE;
				if (v[1] < 0)
					*lastep |= RINF;
				*ep++ = (unsigned char)v[0];
				*ep++ = (unsigned char)(v[1] < 0 ? 0 : v[1]);
				continue;
			}
			if (c >= '1' && c <= '9') {
				if (c - '1' >= nbra)
					return -25;
				NEED(2);
				lastep = ep;
				*ep++ = OBACK;
				*ep++ = (unsigned char)(c - '1');
				continue;
			}
			goto literal;
		default:
		literal:
			NEED(2);
			lastep = ep;
			*ep++ = OCHR;
			*ep++ = (unsigned char)c;
			continue;
		}
	}
	if (depth)
		return -42;
	NEED(1);
	*ep++ = OEOF;
	buf[0] = 'R';
	buf[2] = (unsigned char)nbra;
	buf[3] = (unsigned char)((ep - buf) & 0xff);
	buf[4] = (unsigned char)((ep - buf) >> 8);
	return (int)(ep - buf);
}

struct m {
	const char *bras[NBRA], *kets[NBRA];
	const char *locs, *end;
	int nl;
};

static long one(struct m *m, const unsigned char *ep, const char *lp)
{
	unsigned char c = (unsigned char)*lp;
	switch (OP(*ep)) {
	case OCHR: return c && c == ep[1] ? 1 : -1;
	case ODOT: return c && !(m->nl && c == '\n') ? 1 : -1;
	case OCCL: return c && (ep[1 + (c >> 3)] & (1 << (c & 7))) ? 1 : -1;
	case OBACK: {
		size_t l;
		if (!m->bras[ep[1]] || !m->kets[ep[1]] || m->kets[ep[1]] < m->bras[ep[1]])
			return -1;
		l = (size_t)(m->kets[ep[1]] - m->bras[ep[1]]);
		return strncmp(lp, m->bras[ep[1]], l) ? -1 : (long)l;
	}
	}
	return -1;
}

static size_t elen(const unsigned char *ep)
{
	switch (OP(*ep)) {
	case OCCL: return 33;
	case ODOT: return 1;
	default: return 2;
	}
}

static int adv(struct m *m, const char *lp, const unsigned char *ep)
{
	for (;;) {
		unsigned op = *ep;
		long l;
		switch (OP(op)) {
		case OEOF:
			m->end = lp;
			return 1;
		case ODOL:
			if (*lp && *lp != '\n')
				return 0;
			ep++;
			continue;
		case OBRA:
			m->bras[ep[1]] = lp;
			ep += 2;
			continue;
		case OKET:
			m->kets[ep[1]] = lp;
			ep += 2;
			continue;
		}
		if (!(op & (STAR | RNGE))) {
			if ((l = one(m, ep, lp)) < 0)
				return 0;
			lp += l;
			ep += elen(ep);
			continue;
		}
		{
			const unsigned char *next = ep + elen(ep);
			unsigned long lo = 0, hi = (unsigned long)-1, k = 0;
			long step = 0;
			if (op & RNGE) {
				lo = next[0];
				if (!(op & RINF))
					hi = next[1];
				next += 2;
			}
			while (k < hi && (l = one(m, ep, lp)) >= 0) {
				step = l;
				if (!l) {
					k = lo > k ? lo : k;
					break;
				}
				lp += l;
				k++;
			}
			if (k < lo)
				return 0;
			for (;;) {
				if (lp == m->locs)
					return 0;
				if (adv(m, lp, next))
					return 1;
				if (k == lo || !step)
					return 0;
				lp -= step;
				k--;
			}
		}
	}
}

int __sre_advance(const char *s, const unsigned char *prog, const char *locs, const char **end)
{
	struct m m;
	if (prog[0] != 'R')
		return 0;
	memset(&m, 0, sizeof m);
	m.locs = locs;
	m.nl = (prog[1] & F_NL) != 0;
	if (!adv(&m, s, prog + HDR))
		return 0;
	*end = m.end;
	return 1;
}

int __sre_step(const char *s, const unsigned char *prog, const char *locs,
               const char **start, const char **end)
{
	const char *lp = s;
	for (;;) {
		if (__sre_advance(lp, prog, locs, end)) {
			*start = lp;
			return 1;
		}
		if (!*lp)
			return 0;
		if (prog[1] & F_CIRC) {
			while (*lp && *lp != '\n')
				lp++;
			if (!*lp)
				return 0;
		}
		lp++;
	}
}

char *loc1, *loc2, *locs;

static char *raw;
static size_t rawlen, rawsize;

void __regexp_start(void)
{
	rawlen = 0;
}

void __regexp_add(int c)
{
	if (rawlen == rawsize) {
		char *n = realloc(raw, rawsize ? 2 * rawsize : 256);
		if (!n)
			return;
		raw = n;
		rawsize = rawsize ? 2 * rawsize : 256;
	}
	raw[rawlen++] = (char)c;
}

char *__regexp_end(char *expbuf, const char *endbuf, int *err)
{
	unsigned char *b = (unsigned char *)expbuf;
	int r;
	*err = 0;
	if (!rawlen) {
		if (endbuf - expbuf < HDR || b[0] != 'R') {
			*err = 41;
			return 0;
		}
		return expbuf + (b[3] | b[4] << 8);
	}
	if (rawlen == rawsize && !raw) {
		*err = 50;
		return 0;
	}
	r = __sre_compile(raw, rawlen, b, (size_t)(endbuf - expbuf), 0);
	if (r < 0) {
		*err = -r;
		return 0;
	}
	return expbuf + r;
}

int step(const char *string, const char *expbuf)
{
	const char *s, *e;
	if (!__sre_step(string, (const unsigned char *)expbuf, locs, &s, &e))
		return 0;
	loc1 = (char *)s;
	loc2 = (char *)e;
	return 1;
}

int advance(const char *string, const char *expbuf)
{
	const char *e;
	if (!__sre_advance(string, (const unsigned char *)expbuf, locs, &e))
		return 0;
	loc2 = (char *)e;
	return 1;
}
