/* The printf engine.
 *
 * All conversions of C17 and POSIX.1-2024: flags - + space # 0 ' (the
 * POSIX locale has no grouping), width and precision (also with *),
 * positional arguments (%n$, *m$) up to NL_ARGMAX, length modifiers
 * hh h l ll j z t L, and d i o u x X f F e E g G a A c s p n %.
 * Floating-point output is exact: the value is expanded to all of its
 * decimal digits (internal/fpdigits.c) and rounded to nearest, ties to
 * even.
 */
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include "stdio_impl.h"
#include "fpdigits.h"

/* flags */
#define F_MINUS 1
#define F_PLUS  2
#define F_SPACE 4
#define F_ALT   8
#define F_ZERO  16
#define F_GROUP 32

/* argument types */
enum { T_NONE, T_INT, T_UINT, T_LONG, T_ULONG, T_LLONG, T_ULLONG, T_SCHAR, T_UCHAR,
       T_SHORT, T_USHORT, T_IMAX, T_UMAX, T_SIZE, T_PDIFF, T_DBL, T_LDBL, T_PTR, T_MAXTYPE };

union arg { uintmax_t i; long double f; void *p; };

#include "mb_impl.h"
#include "locale_impl.h"

struct ctx {
	FILE *f;
	size_t count;
	int err;
	const char *dp, *ts, *grp;    /* LC_NUMERIC: decimal point, thousands' separator, grouping */
	size_t dpl, tsl;              /* their bytes */
	long dpw, tsw;                /* their width: characters for wide output, else bytes */
	int wide;                     /* for the wide functions: lengths in characters */
	int mb;                       /* 0: bytes; 1: UTF-8; 2: another multibyte codeset */
	char dpb[32], tsb[32];        /* (wide 2) dp and ts in UTF-8 */
};

/* the bytes of the character at s (wide mode): UTF-8, or the locale's
 * multibyte codeset; a byte that starts none counts alone */
static size_t clen(const struct ctx *c, const char *s, size_t max)
{
	size_t k;
	if (c->mb == 1) {
		for (k = 1; k < max && ((unsigned char)s[k] & 0xc0) == 0x80; k++)
			;
		return k;
	}
	if (c->mb == 2) {
		mbstate_t st = { { 0, 0 } };
		k = __mbrtowc_l(0, s, max, &st, CURRENT_CTYPE);
		if (k != (size_t)-1 && k != (size_t)-2 && k) return k;
	}
	return 1;
}

/* characters in s[0..l) (wide mode) */
static size_t nchars(const struct ctx *c, const char *s, size_t l)
{
	size_t n = 0, i;
	if (!c->mb) return l;
	for (i = 0; i < l; n++) i += clen(c, s + i, l - i);
	return n;
}

/* wide 2 (swprintf): the output is UTF-8 (of any wchar_t value, up to
 * six bytes), so that every wide character is passed through */
size_t __u8enc_any(char *o, unsigned long w)
{
	size_t n, i;
	if (w < 0x80) { o[0] = (char)w; return 1; }
	if (w > 0x7fffffff) return (size_t)-1;
	n = w < 0x800 ? 2 : w < 0x10000 ? 3 : w < 0x200000 ? 4 : w < 0x4000000 ? 5 : 6;
	for (i = n - 1; i > 0; i--, w >>= 6) o[i] = (char)(0x80 | (w & 0x3f));
	o[0] = (char)((0xff00 >> n) | w);
	return n;
}

/* the bytes of wide character w in the output */
static size_t wcout(const struct ctx *c, char *o, wchar_t w)
{
	return c->wide == 2 ? __u8enc_any(o, (unsigned long)w) : __wcrtomb_internal(o, w);
}

/* wide 2: the multibyte string s (of the locale) to UTF-8 in d (of n
 * bytes); its length, or -1 */
static long tou8(char *d, size_t n, const char *s)
{
	size_t k = 0, r;
	mbstate_t st = { { 0, 0 } };
	wchar_t w;
	char b[8];
	while (*s) {
		r = __mbrtowc_l(&w, s, 8, &st, CURRENT_CTYPE);
		if (r == (size_t)-1 || r == (size_t)-2) return -1;
		s += r;
		r = __u8enc_any(b, (unsigned long)w);
		if (k + r >= n) return -1;
		memcpy(d + k, b, r);
		k += r;
	}
	d[k] = 0;
	return (long)k;
}

static void out(struct ctx *c, const char *s, size_t l)
{
	if (!l) return;
	if (c->wide == 2) {
		size_t i;
		for (i = 0; i < l; i++) c->count += ((unsigned char)s[i] & 0xc0) != 0x80;
	} else
		c->count += c->wide ? nchars(c, s, l) : l;
	if (!c->err && !(c->f->flags & F_ERR)) __fwritex((const unsigned char *)s, l, c->f);
}

static void pad(struct ctx *c, char ch, long w, long l, int fl)
{
	char buf[256];
	if ((fl & (F_MINUS | F_ZERO)) || l >= w) return;
	w -= l;
	memset(buf, ch, sizeof buf);
	for (; w >= (long)sizeof buf; w -= (long)sizeof buf) out(c, buf, sizeof buf);
	out(c, buf, (size_t)w);
}

static void padch(struct ctx *c, char ch, long n)
{
	char buf[256];
	if (n <= 0) return;
	memset(buf, ch, sizeof buf);
	for (; n >= (long)sizeof buf; n -= (long)sizeof buf) out(c, buf, sizeof buf);
	out(c, buf, (size_t)n);
}

/* thousands' separators in n integer digits */
static long nseps(const struct ctx *c, long n)
{
	const unsigned char *g = (const unsigned char *)c->grp;
	long k = 0, size;
	if (!c->tsl || !*g) return 0;
	for (;;) {
		size = *g;
		if (!size || size >= 127 || n <= size) return k;
		n -= size;
		k++;
		if (g[1]) g++;
	}
}

/* n digits with the separators */
static void out_grouped(struct ctx *c, const char *d, long n)
{
	long seps = nseps(c, n), pos[64], i, k, size, m = n;
	const unsigned char *g = (const unsigned char *)c->grp;
	if (!seps) { out(c, d, (size_t)n); return; }
	if (seps > 64) seps = 64;
	/* the positions, from the right */
	for (k = 0; k < seps; k++) {
		size = *g;
		m -= size;
		pos[k] = m;
		if (g[1]) g++;
	}
	for (i = 0, k = seps - 1; k >= 0; k--) {
		out(c, d + i, (size_t)(pos[k] - i));
		out(c, c->ts, c->tsl);
		i = pos[k];
	}
	out(c, d + i, (size_t)(n - i));
}

static void fetch(union arg *a, int type, va_list *ap)
{
	switch (type) {
	case T_INT:    a->i = (uintmax_t)(intmax_t)va_arg(*ap, int); break;
	case T_UINT:   a->i = va_arg(*ap, unsigned); break;
	case T_LONG:   a->i = (uintmax_t)(intmax_t)va_arg(*ap, long); break;
	case T_ULONG:  a->i = va_arg(*ap, unsigned long); break;
	case T_LLONG:  a->i = (uintmax_t)(intmax_t)va_arg(*ap, long long); break;
	case T_ULLONG: a->i = va_arg(*ap, unsigned long long); break;
	case T_SCHAR:  a->i = (uintmax_t)(intmax_t)(signed char)va_arg(*ap, int); break;
	case T_UCHAR:  a->i = (unsigned char)va_arg(*ap, int); break;
	case T_SHORT:  a->i = (uintmax_t)(intmax_t)(short)va_arg(*ap, int); break;
	case T_USHORT: a->i = (unsigned short)va_arg(*ap, int); break;
	case T_IMAX:   a->i = (uintmax_t)va_arg(*ap, intmax_t); break;
	case T_UMAX:   a->i = va_arg(*ap, uintmax_t); break;
	case T_SIZE:   a->i = va_arg(*ap, size_t); break;
	case T_PDIFF:  a->i = (uintmax_t)(intmax_t)va_arg(*ap, ptrdiff_t); break;
	case T_DBL:    a->f = va_arg(*ap, double); break;
	case T_LDBL:   a->f = va_arg(*ap, long double); break;
	case T_PTR:    a->p = va_arg(*ap, void *); break;
	}
}

/* the argument type of a conversion with a length modifier */
static int argtype(char conv, int len)
{
	/* len: 0 none, 1 hh, 2 h, 3 l, 4 ll, 5 j, 6 z, 7 t, 8 L */
	static const unsigned char si[] = { T_INT, T_SCHAR, T_SHORT, T_LONG, T_LLONG, T_IMAX, T_SIZE, T_PDIFF, T_LLONG };
	static const unsigned char ui[] = { T_UINT, T_UCHAR, T_USHORT, T_ULONG, T_ULLONG, T_UMAX, T_SIZE, T_PDIFF, T_ULLONG };
	switch (conv) {
	case 'd': case 'i': return si[len];
	case 'o': case 'u': case 'x': case 'X': return ui[len];
	case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
		return len == 8 ? T_LDBL : T_DBL;
	case 'c': return len == 3 ? T_UINT : T_INT;   /* wint_t or int */
	case 's': case 'p': case 'n': return T_PTR;
	}
	return T_NONE;
}

/* ---- integers ---- */

static char *fmt_u(uintmax_t x, char *s, unsigned base, int upper)
{
	const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	do *--s = d[x % base]; while (x /= base);
	return s;
}

/* ---- floating point ---- */

/* round digits d[0..n) to keep digits (may be <= 0), half to even;
 * returns the new digit count; *dexp is increased on carry out */
static int round_digits(char *d, int n, int keep, int *dexp)
{
	int up, i;
	if (keep >= n) return n;
	if (keep < 0) { d[0] = 0; return 0; }
	if (d[keep] > '5') up = 1;
	else if (d[keep] < '5') up = 0;
	else {
		up = 0;
		for (i = keep + 1; i < n; i++) if (d[i] != '0') { up = 1; break; }
		/* an exact tie: to even (the digit before 0.d1 counts as 0) */
		if (!up) up = keep > 0 ? (d[keep - 1] - '0') & 1 : 0;
	}
	n = keep;
	if (up) {
		for (i = n - 1; i >= 0 && d[i] == '9'; i--) d[i] = '0';
		if (i >= 0) d[i]++;
		else { d[0] = '1'; n = 1; (*dexp)++; }   /* 0.999 -> 0.1 * 10 */
	}
	while (n > 0 && d[n - 1] == '0') n--;
	d[n] = 0;
	return n;
}

static void fmt_hex(struct ctx *c, long double x, long w, int p, int fl, char t, int is_ld)
{
	uint64_t m;
	int e, sign, cls, ndig, i, lead;
	char buf[40], ebuf[16], *es;
	const char *prefix = "-";
	const char *xd = (t & 32) ? "0123456789abcdef" : "0123456789ABCDEF";
	char digs[17];

	__ld_parts(x, &m, &e, &sign, &cls);
	if (!sign) prefix = (fl & F_PLUS) ? "+" : (fl & F_SPACE) ? " " : "";
	if (cls == FP_CLS_ZERO) { m = 0; e = 0; }

	if (!is_ld) {
		/* a double: 1 + 52 bits, as 1.hhhhhhhhhhhhh, subnormals as 0.h... */
		int de = 0;
		uint64_t frac;
		if (cls == FP_CLS_ZERO) { lead = 0; frac = 0; }
		else {
			while (!(m >> 63)) { m <<= 1; e--; }
			de = e + 63;   /* the exponent of the top bit */
			if (de < -1022) {   /* subnormal double */
				int sh = -1022 - de;
				lead = 0;
				frac = (m >> 11) >> sh;      /* 53 bits, shifted */
				frac &= 0xfffffffffffffULL;
				de = -1022;
				if (sh > 52) frac = 0;
			} else {
				lead = 1;
				frac = (m >> 11) & 0xfffffffffffffULL;
			}
		}
		for (i = 0; i < 13; i++) digs[i] = (char)((frac >> (48 - 4 * i)) & 15);
		ndig = 13;
		e = de;
	} else {
		/* long double: the leading hex digit holds the top 4 bits */
		/* value = m * 2^e = (m / 2^60) * 2^(e + 60): the leading digit is
		 * the top 4 bits of the normalised mantissa (0x8p-3 for 1.0L) */
		if (cls == FP_CLS_ZERO) { lead = 0; e = 0; }
		else {
			/* subnormals (as glibc) unnormalised: 0x0.0...p-16385 */
			while (!(m >> 63) && e > -16445) { m <<= 1; e--; }
			lead = (int)(m >> 60);
			e += 60;
		}
		for (i = 0; i < 15; i++) digs[i] = (char)((m >> (56 - 4 * i)) & 15);
		ndig = 15;
	}
	/* precision: round half to even on the hex digits */
	if (p >= 0 && p < ndig) {
		int up = 0, j;
		if (digs[p] > 8) up = 1;
		else if (digs[p] == 8) {
			for (j = p + 1; j < ndig; j++) if (digs[j]) { up = 1; break; }
			if (!up) up = (p ? digs[p - 1] : lead) & 1;
		}
		ndig = p;
		if (up) {
			for (j = p - 1; j >= 0 && digs[j] == 15; j--) digs[j] = 0;
			if (j >= 0) digs[j]++;
			else {
				/* the carry goes into the leading digit (0x1.f -> 0x2), as
				 * glibc prints it; a long double's 0xf.f becomes 0x10 = 0x1p+4 */
				lead++;
				if (lead == 16) { lead = 1; e += 4; }
			}
		}
	}
	if (p < 0) while (ndig > 0 && !digs[ndig - 1]) ndig--;
	/* assemble */
	{
		char *s = buf;
		long l, numlen;
		int prec = p < 0 ? ndig : p;
		*s++ = '0'; *s++ = (char)(t + ('x' - 'a'));
		*s++ = xd[lead];
		if (prec || (fl & F_ALT)) *s++ = '.';   /* replaced below by the decimal point */
		for (i = 0; i < ndig && i < prec; i++) *s++ = xd[(int)digs[i]];
		es = fmt_u((uintmax_t)(e < 0 ? -e : e), ebuf + sizeof ebuf, 10, 0);
		*--es = e < 0 ? '-' : '+';
		*--es = (char)(t + ('p' - 'a'));
		numlen = (long)(s - buf) + (prec > ndig ? prec - ndig : 0) + (long)(ebuf + sizeof ebuf - es);
		if (prec || (fl & F_ALT)) numlen += c->dpw - 1;
		l = (long)strlen(prefix) + numlen;
		pad(c, ' ', w, l, fl);
		out(c, prefix, strlen(prefix));
		out(c, buf, 2);
		pad(c, '0', w, l, fl ^ F_ZERO);
		out(c, buf + 2, 1);
		if (prec || (fl & F_ALT)) {
			out(c, c->dp, c->dpl);
			out(c, buf + 4, (size_t)(s - buf - 4));
		}
		padch(c, '0', prec - ndig);
		out(c, es, (size_t)(ebuf + sizeof ebuf - es));
		pad(c, ' ', w, l, fl ^ F_MINUS);
	}
}

static void fmt_fp(struct ctx *c, long double x, long w, int p, int fl, char t, int is_ld)
{
	uint64_t m;
	int e, sign, cls, n, dexp;
	char digits[FP_DIGITS_MAX];
	const char *prefix;

	__ld_parts(x, &m, &e, &sign, &cls);
	prefix = sign ? "-" : (fl & F_PLUS) ? "+" : (fl & F_SPACE) ? " " : "";

	if (cls == FP_CLS_INF || cls == FP_CLS_NAN) {
		const char *s = cls == FP_CLS_NAN ? ((t & 32) ? "nan" : "NAN") : ((t & 32) ? "inf" : "INF");
		long l = (long)strlen(prefix) + 3;
		pad(c, ' ', w, l, fl & ~F_ZERO);
		out(c, prefix, strlen(prefix));
		out(c, s, 3);
		pad(c, ' ', w, l, (fl & ~F_ZERO) ^ F_MINUS);
		return;
	}
	if ((t | 32) == 'a') { fmt_hex(c, x, w, p, fl, t, is_ld); return; }
	if (p < 0) p = 6;

	n = cls == FP_CLS_ZERO ? 0 : __fp_digits(m, e, digits, &dexp);
	if (!n) dexp = 1;

	if ((t | 32) == 'g') {
		int P = p ? p : 1, X;
		int n2, dx = dexp;
		n2 = round_digits(digits, n, P, &dx);
		X = n2 ? dx - 1 : 0;
		n = n2; dexp = dx;
		if (P > X && X >= -4) { t = (char)(t - ('g' - 'f')); p = P - 1 - X; }
		else { t = (char)(t - ('g' - 'e')); p = P - 1; }
		if (!(fl & F_ALT)) {
			/* trailing zeros are removed: limit the precision to the digits */
			int have = (t | 32) == 'f' ? n - dexp : n - 1;
			if (have < 0) have = 0;
			if (p > have) p = have;
		}
	}

	if ((t | 32) == 'f') {
		int keep = dexp + p;
		long intdig, l;
		n = round_digits(digits, n, keep, &dexp);
		if (!n) dexp = 1;
		intdig = dexp > 0 ? dexp : 1;
		if (fl & F_GROUP) intdig += nseps(c, intdig) * c->tsw;
		l = (long)strlen(prefix) + intdig + (p || (fl & F_ALT) ? c->dpw : 0) + p;
		pad(c, ' ', w, l, fl);
		out(c, prefix, strlen(prefix));
		pad(c, '0', w, l, fl ^ F_ZERO);
		/* integer part */
		if (dexp <= 0) out(c, "0", 1);
		else if ((fl & F_GROUP) && nseps(c, dexp)) {
			char ip[FP_DIGITS_MAX];
			int k = n < dexp ? n : dexp;
			memcpy(ip, digits, (size_t)k);
			memset(ip + k, '0', (size_t)(dexp - k));
			out_grouped(c, ip, dexp);
		} else {
			int k = n < dexp ? n : dexp;
			out(c, digits, (size_t)k);
			padch(c, '0', dexp - k);
		}
		if (p || (fl & F_ALT)) out(c, c->dp, c->dpl);
		/* fraction: p digits */
		{
			long z = dexp < 0 ? -dexp : 0;     /* zeros right after the point */
			long from = dexp > 0 ? dexp : 0;  /* index of the first fraction digit */
			long avail = n > from ? n - from : 0;
			if (z > p) z = p;
			padch(c, '0', z);
			if (avail > p - z) avail = p - z;
			out(c, digits + from, (size_t)avail);
			padch(c, '0', p - z - avail);
		}
		pad(c, ' ', w, l, fl ^ F_MINUS);
	} else {
		/* %e */
		char ebuf[16], *es;
		int x10;
		long l;
		n = round_digits(digits, n, p + 1, &dexp);
		x10 = n ? dexp - 1 : 0;
		es = fmt_u((uintmax_t)(x10 < 0 ? -x10 : x10), ebuf + sizeof ebuf, 10, 0);
		if (ebuf + sizeof ebuf - es < 2) *--es = '0';
		*--es = x10 < 0 ? '-' : '+';
		*--es = (char)(t & 32 ? 'e' : 'E');
		l = (long)strlen(prefix) + 1 + (p || (fl & F_ALT) ? c->dpw : 0) + p + (long)(ebuf + sizeof ebuf - es);
		pad(c, ' ', w, l, fl);
		out(c, prefix, strlen(prefix));
		pad(c, '0', w, l, fl ^ F_ZERO);
		out(c, n ? digits : "0", 1);
		if (p || (fl & F_ALT)) out(c, c->dp, c->dpl);
		{
			long avail = n > 1 ? n - 1 : 0;
			if (avail > p) avail = p;
			out(c, digits + 1, (size_t)avail);
			padch(c, '0', p - avail);
		}
		out(c, es, (size_t)(ebuf + sizeof ebuf - es));
		pad(c, ' ', w, l, fl ^ F_MINUS);
	}
}

/* ---- the engine ---- */

static int getint(const char **s)
{
	int i = 0;
	for (; (unsigned)**s - '0' < 10; (*s)++) {
		if (i > (INT_MAX - 9) / 10) i = -1;   /* overflow marker */
		else if (i >= 0) i = 10 * i + (**s - '0');
	}
	return i;
}

/* One pass over the format.  With nl_type set, only records argument
 * types (pass 1 for positional arguments); otherwise prints. */
static int printf_core(struct ctx *c, const char *fmt, va_list *ap, union arg *nl_arg, int *nl_type)
{
	const char *s = fmt, *a, *z;
	int argpos, positional = -1;   /* unknown */
	unsigned sequential = 0;

	for (;;) {
		int fl = 0, len = 0, type;
		long w = 0, p = -1;
		char t;
		union arg arg;
		char buf[sizeof(uintmax_t) * 3 + 3], *ns = 0;
		const char *prefix = "";
		size_t plen;
		long l;

		/* literal text */
		for (a = s; *s && *s != '%'; s++);
		for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2);
		if (!nl_type) out(c, a, (size_t)(z - a));
		if (!*s) break;
		if (*s != '%') continue;   /* after "%%": more literal text */
		s++;   /* past '%' */

		/* %n$ */
		argpos = -1;
		if ((unsigned)s[0] - '0' < 10 && s[1] == '$') { argpos = s[0] - '0'; s += 2; }
		else if ((unsigned)s[0] - '0' < 10) {
			const char *q = s;
			int v = getint(&q);
			if (*q == '$') { argpos = v; s = q + 1; }
		}
		if (argpos >= 0) {
			if (positional == 0 || argpos < 1 || argpos > NL_ARGMAX) goto inval;
			positional = 1;
		} else if (positional == 1) goto inval;
		else positional = 0;

		/* flags */
		for (;; s++) {
			if (*s == '-') fl |= F_MINUS;
			else if (*s == '+') fl |= F_PLUS;
			else if (*s == ' ') fl |= F_SPACE;
			else if (*s == '#') fl |= F_ALT;
			else if (*s == '0') fl |= F_ZERO;
			else if (*s == '\'') fl |= F_GROUP;
			else break;
		}

		/* width */
		if (*s == '*') {
			s++;
			if ((unsigned)*s - '0' < 10) {
				int v = getint(&s);
				if (*s != '$' || !positional || v < 1 || v > NL_ARGMAX) goto inval;
				s++;
				if (nl_type) { nl_type[v] = T_INT; w = 0; }
				else w = (long)(int)nl_arg[v].i;
			} else {
				if (positional == 1) goto inval;
				positional = 0;
				if (!nl_type) w = va_arg(*ap, int);
			}
			if (w < 0) { fl |= F_MINUS; w = -w; }
		} else {
			int v = getint(&s);
			if (v < 0) goto overflow;
			w = v;
		}

		/* precision */
		if (*s == '.') {
			s++;
			if (*s == '*') {
				s++;
				if ((unsigned)*s - '0' < 10) {
					int v = getint(&s);
					if (*s != '$' || !positional || v < 1 || v > NL_ARGMAX) goto inval;
					s++;
					if (nl_type) { nl_type[v] = T_INT; p = 0; }
					else p = (long)(int)nl_arg[v].i;
				} else {
					if (positional == 1) goto inval;
					positional = 0;
					if (!nl_type) p = va_arg(*ap, int);
				}
				if (p < 0) p = -1;
			} else {
				int v = getint(&s);
				if (v < 0) goto overflow;
				p = v;
			}
		}

		/* length */
		switch (*s) {
		case 'h': s++; if (*s == 'h') { s++; len = 1; } else len = 2; break;
		case 'l': s++; if (*s == 'l') { s++; len = 4; } else len = 3; break;
		case 'j': s++; len = 5; break;
		case 'z': s++; len = 6; break;
		case 't': s++; len = 7; break;
		case 'L': s++; len = 8; break;
		case 'q': s++; len = 4; break;
		}
		t = *s++;
		if (t == 'C') { t = 'c'; len = 3; }
		else if (t == 'S') { t = 's'; len = 3; }
		type = argtype(t, len);
		if (type == T_NONE) goto inval;

		if (nl_type) {
			if (argpos > 0) nl_type[argpos] = type;
			continue;
		}
		if (argpos > 0) arg = nl_arg[argpos];
		else fetch(&arg, type, ap);
		sequential++;

		if (fl & F_MINUS) fl &= ~F_ZERO;

		switch (t) {
		case 'n':
			if (!arg.p) break;
			switch (len) {
			case 0: *(int *)arg.p = (int)c->count; break;
			case 1: *(signed char *)arg.p = (signed char)c->count; break;
			case 2: *(short *)arg.p = (short)c->count; break;
			case 3: *(long *)arg.p = (long)c->count; break;
			case 4: case 8: *(long long *)arg.p = (long long)c->count; break;
			case 5: *(intmax_t *)arg.p = (intmax_t)c->count; break;
			case 6: *(size_t *)arg.p = c->count; break;
			case 7: *(ptrdiff_t *)arg.p = (ptrdiff_t)c->count; break;
			}
			continue;
		case 'p':
			/* as %#lx; a null pointer prints as (nil), as on other systems */
			if (!arg.p) { ns = "(nil)"; plen = 5; p = -1; goto string; }
			fl |= F_ALT; t = 'x'; arg.i = (uintmax_t)(uintptr_t)arg.p;
			/* fall through */
		case 'x': case 'X': case 'o': case 'u': case 'd': case 'i': {
			char *e = buf + sizeof buf;
			uintmax_t v = arg.i;
			if (t == 'd' || t == 'i') {
				if ((intmax_t)v < 0) { v = -v; prefix = "-"; }
				else if (fl & F_PLUS) prefix = "+";
				else if (fl & F_SPACE) prefix = " ";
			}
			ns = fmt_u(v, e, t == 'o' ? 8 : (t | 32) == 'x' ? 16 : 10, t == 'X');
			if ((t | 32) == 'x' && (fl & F_ALT) && v) prefix = t == 'X' ? "0X" : "0x";
			if (p >= 0) fl &= ~F_ZERO;
			if (!v && !p) ns = e;   /* zero with precision 0: no digits */
			plen = (size_t)(e - ns);
			/* %#o: the first digit is a zero */
			if (t == 'o' && (fl & F_ALT) && (v || !plen) && (long)plen >= p) p = (long)plen + 1;
			if (p < (long)plen) p = (long)plen;
			if ((fl & F_GROUP) && (t == 'd' || t == 'i' || t == 'u') && p == (long)plen) {
				/* thousands' separators (the digits only, not zeros of a precision) */
				long seps = nseps(c, (long)plen) * c->tsw;
				l = (long)strlen(prefix) + p + seps;
				pad(c, ' ', w, l, fl);
				out(c, prefix, strlen(prefix));
				pad(c, '0', w, l, fl ^ F_ZERO);
				out_grouped(c, ns, (long)plen);
				pad(c, ' ', w, l, fl ^ F_MINUS);
				continue;
			}
			l = (long)strlen(prefix) + p;
			if (w < l) w = l;
			pad(c, ' ', w, l, fl);
			out(c, prefix, strlen(prefix));
			pad(c, '0', w, l, fl ^ F_ZERO);
			padch(c, '0', p - (long)plen);
			out(c, ns, plen);
			pad(c, ' ', w, l, fl ^ F_MINUS);
			continue;
		}
		case 'c':
			if (len == 3) {
				char mb[8];
				size_t k = wcout(c, mb, (wchar_t)arg.i);
				if (k == (size_t)-1) goto ilseq;
				ns = mb; plen = k; p = -1;
				l = c->wide ? 1 : (long)k;
				pad(c, ' ', w, l, fl & ~F_ZERO);
				out(c, mb, k);
				pad(c, ' ', w, l, (fl & ~F_ZERO) ^ F_MINUS);
				continue;
			}
			if (c->wide == 2) {
				/* (the int as btowc) */
				char mb[8], b1 = (char)arg.i;
				mbstate_t st = { { 0, 0 } };
				wchar_t wc;
				size_t k = __mbrtowc_l(&wc, &b1, 1, &st, CURRENT_CTYPE);
				if (k == (size_t)-1 || k == (size_t)-2) goto ilseq;
				k = __u8enc_any(mb, (unsigned long)wc);
				pad(c, ' ', w, 1, fl & ~F_ZERO);
				out(c, mb, k);
				pad(c, ' ', w, 1, (fl & ~F_ZERO) ^ F_MINUS);
				continue;
			}
			buf[0] = (char)arg.i;
			ns = buf; plen = 1;
			pad(c, ' ', w, 1, fl & ~F_ZERO);
			out(c, buf, 1);
			pad(c, ' ', w, 1, (fl & ~F_ZERO) ^ F_MINUS);
			continue;
		case 's':
			if (len == 3) {
				/* wide string: converted; the precision counts bytes */
				const wchar_t *ws = arg.p ? arg.p : L"(null)";
				size_t total = 0, k, i;
				char mb[8];
				for (i = 0; ws[i]; i++) {
					k = wcout(c, mb, ws[i]);
					if (k == (size_t)-1) goto ilseq;
					/* the precision counts bytes; for the wide functions, characters */
					if (p >= 0 && (c->wide ? i + 1 > (size_t)p : total + k > (size_t)p)) break;
					total += k;
				}
				l = c->wide ? (long)i : (long)total;
				pad(c, ' ', w, l, fl & ~F_ZERO);
				for (i = 0, k = 0; k < total; i++) {
					size_t m2 = wcout(c, mb, ws[i]);
					out(c, mb, m2);
					k += m2;
				}
				pad(c, ' ', w, l, (fl & ~F_ZERO) ^ F_MINUS);
				continue;
			}
			ns = arg.p ? arg.p : "(null)";
			if (c->wide && c->mb) {
				/* the precision counts characters */
				size_t nc = 0;
				for (plen = 0; ns[plen]; nc++) {
					if (p >= 0 && nc == (size_t)p) break;
					plen += clen(c, ns + plen, 4);   /* (stops at the null byte) */
				}
			} else if (p >= 0) for (plen = 0; plen < (size_t)p && ns[plen]; plen++);
			else plen = strlen(ns);
		string:
			l = c->wide ? (long)nchars(c, ns, plen) : (long)plen;
			pad(c, ' ', w, l, fl & ~F_ZERO);
			if (c->wide == 2) {
				/* the locale's characters to UTF-8 */
				size_t i2 = 0, k;
				char mb[8];
				while (i2 < plen) {
					mbstate_t st = { { 0, 0 } };
					wchar_t wc;
					k = __mbrtowc_l(&wc, ns + i2, plen - i2, &st, CURRENT_CTYPE);
					if (k == (size_t)-1 || k == (size_t)-2) goto ilseq;
					if (!k) break;
					i2 += k;
					out(c, mb, __u8enc_any(mb, (unsigned long)wc));
				}
			} else
				out(c, ns, plen);
			pad(c, ' ', w, l, (fl & ~F_ZERO) ^ F_MINUS);
			continue;
		default:   /* floating point */
			if (p > INT_MAX - 1000) goto overflow;
			fmt_fp(c, arg.f, w, (int)p, fl, t, len == 8);
			continue;
		}
	}
	(void)sequential;
	return 0;
inval:
	errno = EINVAL;
	return -1;
overflow:
	errno = EOVERFLOW;
	return -1;
ilseq:
	errno = EILSEQ;
	return -1;
}

static int do_printf(FILE *f, const char *fmt, va_list ap, int wide)
{
	struct ctx c;
	union arg nl_arg[NL_ARGMAX + 1];
	int nl_type[NL_ARGMAX + 1] = { 0 };
	va_list ap2;
	int r, i, positional = 0, olderr;
	const char *s;

	/* positional arguments? */
	for (s = fmt; *s; s++) {
		if (*s != '%') continue;
		if (s[1] == '%') { s++; continue; }
		{
			const char *q = s + 1;
			while ((unsigned)*q - '0' < 10) q++;
			positional = q > s + 1 && *q == '$';
			break;
		}
	}

	c.f = f;
	c.count = 0;
	c.err = 0;
	{
		const struct __lcat *num = CURRENT_LOCALE->cat[LC_NUMERIC];
		c.dp = __lc_str(num, 0);
		c.ts = __lc_str(num, 1);
		c.grp = __lc_str(num, 2);
		if (!*c.dp) c.dp = ".";
		c.dpl = strlen(c.dp);
		c.tsl = strlen(c.ts);
		c.wide = wide;
		c.mb = CURRENT_UTF8 ? 1 : CURRENT_CTYPE->cs == CS_MB ? 2 : 0;
		c.dpw = (long)(wide ? nchars(&c, c.dp, c.dpl) : c.dpl);
		c.tsw = (long)(wide ? nchars(&c, c.ts, c.tsl) : c.tsl);
		if (wide == 2 && num->n > 4 && num->elem[3] && num->elem[4]) {
			/* the locale's wide decimal point and separator (as glibc:
			 * they may differ from the multibyte ones) */
			uint32_t dw, tw;
			memcpy(&dw, num->elem[3], 4);
			memcpy(&tw, num->elem[4], 4);
			if (!dw) dw = '.';
			c.dpl = __u8enc_any(c.dpb, dw);
			c.dp = c.dpb;
			c.dpw = 1;
			c.tsl = tw ? __u8enc_any(c.tsb, tw) : 0;
			c.ts = c.tsb;
			c.tsw = tw != 0;
		} else if (wide == 2) {
			long k;
			if ((k = tou8(c.dpb, sizeof c.dpb, c.dp)) < 0) { c.dp = "."; c.dpl = 1; c.dpw = 1; }
			else { c.dp = c.dpb; c.dpl = (size_t)k; }
			if ((k = tou8(c.tsb, sizeof c.tsb, c.ts)) < 0) { c.ts = ""; c.tsl = 0; c.tsw = 0; }
			else { c.ts = c.tsb; c.tsl = (size_t)k; }
		}
	}
	va_copy(ap2, ap);
	if (positional) {
		/* pass 1: the types; then fetch the arguments in order */
		if (printf_core(&c, fmt, &ap2, 0, nl_type) < 0) { va_end(ap2); return -1; }
		for (i = 1; i <= NL_ARGMAX && nl_type[i]; i++) fetch(&nl_arg[i], nl_type[i], &ap2);
		for (; i <= NL_ARGMAX; i++)
			if (nl_type[i]) { va_end(ap2); errno = EINVAL; return -1; }   /* a gap */
	}
	if (!wide) f->mode |= f->mode - 1;
	else if (!f->mode) f->mode = 1;
	if (!f->wend && __towrite(f)) { va_end(ap2); return -1; }
	/* an error of an earlier call does not stop this one (as glibc and
	 * musl): only an error now makes the result -1 */
	olderr = f->flags & F_ERR;
	f->flags &= ~F_ERR;
	r = printf_core(&c, fmt, &ap2, nl_arg, 0);
	va_end(ap2);
	if (f->flags & F_ERR) r = -1;
	f->flags |= olderr;
	if (r < 0) return -1;
	if (c.count > INT_MAX) { errno = EOVERFLOW; return -1; }
	return (int)c.count;
}

int __vfprintf(FILE *f, const char *fmt, va_list ap) { return do_printf(f, fmt, ap, 0); }

/* for the wide functions: the format converted to multibyte; the
 * result counts characters */
int __vfwprintf_mb(FILE *f, const char *fmt, va_list ap) { return do_printf(f, fmt, ap, 1); }

int vfprintf(FILE *__RESTRICT f, const char *__RESTRICT fmt, va_list ap)
{
	int r;
	FLOCK(f);
	r = __vfprintf(f, fmt, ap);
	FUNLOCK(f);
	return r;
}

/* for swprintf: the format and the output in UTF-8 (of any wchar_t) */
int __vfwprintf_u8(FILE *f, const char *fmt, va_list ap) { return do_printf(f, fmt, ap, 2); }
