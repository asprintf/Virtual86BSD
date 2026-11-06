#include <monetary.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "locale_impl.h"

/* LC_MONETARY elements */
enum { M_INTCUR, M_CUR, M_DP, M_TS, M_GROUP, M_POS, M_NEG, M_INTFRAC, M_FRAC, M_PCS, M_PSEP,
       M_NCS, M_NSEP, M_PPOSN, M_NPOSN, M_CRNCY, M_IPCS, M_IPSEP, M_INCS, M_INSEP, M_IPPOSN, M_INPOSN };

struct out { char *s; size_t n, max; int over; };

static void put(struct out *o, const char *s, size_t l)
{
	for (; l; l--, s++) { if (o->n < o->max) o->s[o->n++] = *s; else o->over = 1; }
}

static void putc_(struct out *o, char c) { put(o, &c, 1); }

/* a value of LC_MONETARY: -1 for unspecified (CHAR_MAX) */
static int mval(const struct __lcat *m, unsigned idx)
{
	int v = (signed char)*__lc_str(m, idx);
	return v == CHAR_MAX ? -1 : v;
}

/* the digits of the integer part with separators, into b (from the end) */
static size_t group(char *b, const char *dig, size_t n, const char *ts, const char *g)
{
	size_t k = 0, i, tl = strlen(ts), run = 0;
	const unsigned char *gp = (const unsigned char *)g;
	int size = *gp && *gp < CHAR_MAX ? *gp : 0;
	for (i = n; i > 0; i--) {
		if (size && run == (size_t)size && tl) {
			size_t j;
			for (j = tl; j > 0; j--) b[k++] = ts[j - 1];
			run = 0;
			if (gp[1]) { gp++; size = *gp && *gp < CHAR_MAX ? *gp : 0; }
		}
		b[k++] = dig[i - 1];
		run++;
	}
	/* reverse */
	for (i = 0; i < k / 2; i++) { char t = b[i]; b[i] = b[k - 1 - i]; b[k - 1 - i] = t; }
	return k;
}

static ssize_t vstrfmon_l(char *s, size_t max, locale_t loc, const char *fmt, va_list ap)
{
	const struct __lcat *m;
	struct out o = { s, 0, max, 0 };
	if (loc == LC_GLOBAL_LOCALE) loc = &__global_locale;
	m = loc->cat[LC_MONETARY];
	for (; *fmt; fmt++) {
		char fill = ' ', conv;
		int nogroup = 0, style = 0, nocur = 0, left = 0, width = 0, lprec = -1, rprec = -1, intl, neg;
		double v;
		char num[512], dig[400], grp[800], field[1200];
		const char *cur, *dp, *p;
		int k;
		size_t nl, fl;
		if (*fmt != '%') { putc_(&o, *fmt); continue; }
		fmt++;
		if (*fmt == '%') { putc_(&o, '%'); continue; }
		for (;; fmt++) {
			if (*fmt == '=' && fmt[1]) fill = *++fmt;
			else if (*fmt == '^') nogroup = 1;
			else if (*fmt == '+') { if (style) goto inval; style = '+'; }
			else if (*fmt == '(') { if (style) goto inval; style = '('; }
			else if (*fmt == '!') nocur = 1;
			else if (*fmt == '-') left = 1;
			else break;
		}
		while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
		if (*fmt == '#') { lprec = 0; fmt++; while (*fmt >= '0' && *fmt <= '9') lprec = lprec * 10 + (*fmt++ - '0'); }
		if (*fmt == '.') { rprec = 0; fmt++; while (*fmt >= '0' && *fmt <= '9') rprec = rprec * 10 + (*fmt++ - '0'); }
		conv = *fmt;
		if (conv != 'i' && conv != 'n') goto inval;
		intl = conv == 'i';
		v = va_arg(ap, double);
		neg = v < 0;   /* (as glibc: -0 is not negative) */
		if (neg) v = -v;
		if (rprec < 0) {
			int fd = *__lc_str(m, intl ? M_INTFRAC : M_FRAC);
			rprec = fd == CHAR_MAX || fd < 0 ? 2 : fd;
		}
		if (rprec > 100 || width > 1000 || lprec > 300) goto inval;
		snprintf(num, sizeof num, "%.*f", rprec, v);
		/* the integer digits and the fraction (the radix character of
		 * LC_NUMERIC between them) */
		nl = strspn(num, "0123456789");
		p = num + strlen(num) - rprec - 1;   /* (just before the fraction) */
		if (nl >= sizeof dig) goto inval;
		memcpy(dig, num, nl);
		if (!nogroup) nl = group(grp, dig, nl, __lc_str(m, M_TS), __lc_str(m, M_GROUP));
		else memcpy(grp, dig, nl);
		/* the left precision: fill characters before the digits, as many
		 * as make the width of lprec digits (with their separators) */
		fl = 0;
		if (lprec > 0 && (size_t)lprec > strspn(num, "0123456789")) {
			char z[400], zg[800];
			size_t zl = (size_t)lprec;
			int need;
			memset(z, '0', zl);
			if (!nogroup) zl = group(zg, z, zl, __lc_str(m, M_TS), __lc_str(m, M_GROUP));
			need = (int)zl - (int)nl;
			for (k = 0; k < need; k++) field[fl++] = fill;
		}
		memcpy(field + fl, grp, nl);
		fl += nl;
		if (rprec) {
			dp = __lc_str(m, M_DP);   /* empty in the POSIX locale: none */
			memcpy(field + fl, dp, strlen(dp));
			fl += strlen(dp);
			memcpy(field + fl, p + 1, (size_t)rprec);
			fl += (size_t)rprec;
		}
		field[fl] = 0;
		/* the currency symbol, the sign and their places: as glibc
		 * (stdlib/strfmon_l.c), which also aligns a positive and a
		 * negative amount when there is a left precision */
		{
			char out[1600], isym[4];
			size_t ol = 0, clen;
			const char *sgn, *osgn;
			int cs, sep, posn, ocs, osep, oposn, pposn, nposn, lpad = 0, lb, olb;
			char space = ' ';
			if (intl) {
				const char *ic = __lc_str(m, M_INTCUR);
				strncpy(isym, ic, 3);
				isym[3] = 0;
				cur = isym;
				clen = 3;
				space = strlen(ic) > 3 ? ic[3] : 0;
			} else {
				cur = __lc_str(m, M_CUR);
				clen = strlen(cur);
			}
			if (style == '+') {
				/* (POSIX: both signs empty fail; glibc gives "-") */
				if (!*__lc_str(m, M_POS) && !*__lc_str(m, M_NEG)) goto inval;
				pposn = mval(m, M_PPOSN); nposn = mval(m, M_NPOSN);
			}
			else if (style == '(') pposn = nposn = 0;
			else { pposn = mval(m, intl ? M_IPPOSN : M_PPOSN); nposn = mval(m, intl ? M_INPOSN : M_NPOSN); }
			{
				const char *ps = __lc_str(m, M_POS), *ns = __lc_str(m, M_NEG);
				if (!*ns) ns = "-";
				int pcs = mval(m, intl ? M_IPCS : M_PCS), psep = mval(m, intl ? M_IPSEP : M_PSEP);
				int ncs = mval(m, intl ? M_INCS : M_NCS), nsep = mval(m, intl ? M_INSEP : M_NSEP);
				if (neg) { sgn = ns; cs = ncs; sep = nsep; posn = nposn; osgn = ps; ocs = pcs; osep = psep; oposn = pposn; }
				else { sgn = ps; cs = pcs; sep = psep; posn = pposn; osgn = ns; ocs = ncs; osep = nsep; oposn = nposn; }
			}
			cs = cs != 0; ocs = ocs != 0;
			if (sep == -1) sep = 0;
			if (osep == -1) osep = 0;
			if (posn == -1) posn = 1;
			if (oposn == -1) oposn = 1;
			/* sign and symbol not adjacent: no separator */
			if (sep == 2 && (posn == 0 || (posn == 1 && !cs) || (posn == 2 && cs))) sep = 0;
			if (osep == 2 && (oposn == 0 || (oposn == 1 && !ocs) || (oposn == 2 && ocs))) osep = 0;
			if (lprec >= 0) {
				lb = olb = 0;
				if (cs) lb += (int)clen + (sep != 0);
				if (ocs) olb += (int)clen + (osep != 0);
				if (posn == 0 && neg) lb++;
				else if (posn == 1 || (cs && (posn == 3 || posn == 4))) lb += (int)strlen(sgn);
				if (oposn == 0 && !neg) olb++;
				else if (oposn == 1 || (ocs && (oposn == 3 || oposn == 4))) olb += (int)strlen(osgn);
				if (olb > lb) lpad = olb - lb;
			}
#define ADD(str, len) do { size_t l_ = (len); if (ol + l_ <= sizeof out) { memcpy(out + ol, str, l_); ol += l_; } } while (0)
#define ADDC(ch) do { char c_ = (ch); ADD(&c_, 1); } while (0)
			for (k = 0; k < lpad; k++) ADDC(' ');
			if (posn == 0 && neg) ADDC('(');
			if (cs) {
				if (posn != 0 && posn != 2 && posn != 4 && posn != 5) {
					ADD(sgn, strlen(sgn));
					if (sep == 2) ADDC(' ');
				}
				if (!nocur) ADD(cur, strlen(cur));
				if (posn == 4) {
					if (!nocur && sep == 2) ADDC(space);
					ADD(sgn, strlen(sgn));
					if (sep == 1) ADDC(' ');
				} else if (!nocur && sep == 1) ADDC(space);
			} else if (posn != 0 && posn != 2 && posn != 3 && posn != 4 && posn != 5)
				ADD(sgn, strlen(sgn));
			ADD(field, fl);
			if (!cs) {
				if (posn == 3) {
					if (sep == 1) ADDC(' ');
					ADD(sgn, strlen(sgn));
				}
				if (!nocur) {
					if ((posn == 3 && sep == 2) || (posn == 4 && sep == 1) || (posn == 2 && sep == 1)
					    || (posn == 1 && sep == 1) || (posn == 0 && sep == 1))
						ADDC(space);
					ADD(cur, strnlen(cur, clen));
				}
				if (posn == 4) {
					if (sep == 2) ADDC(' ');
					ADD(sgn, strlen(sgn));
				}
			}
			if (posn == 2) {
				if (sep == 2) ADDC(' ');
				ADD(sgn, strlen(sgn));
			}
			if (posn == 0 && neg) ADDC(')');
#undef ADD
#undef ADDC
			if ((size_t)width > ol && !left) for (k = 0; k < width - (int)ol; k++) putc_(&o, ' ');
			put(&o, out, ol);
			if ((size_t)width > ol && left) for (k = 0; k < width - (int)ol; k++) putc_(&o, ' ');
		}
	}
	if (o.over || o.n >= max) { errno = E2BIG; return -1; }
	s[o.n] = 0;
	return (ssize_t)o.n;
inval:
	errno = EINVAL;
	return -1;
}

ssize_t strfmon_l(char *__RESTRICT s, size_t max, locale_t l, const char *__RESTRICT fmt, ...)
{
	va_list ap;
	ssize_t r;
	va_start(ap, fmt);
	r = vstrfmon_l(s, max, l, fmt, ap);
	va_end(ap);
	return r;
}

ssize_t strfmon(char *__RESTRICT s, size_t max, const char *__RESTRICT fmt, ...)
{
	va_list ap;
	ssize_t r;
	va_start(ap, fmt);
	r = vstrfmon_l(s, max, CURRENT_LOCALE, fmt, ap);
	va_end(ap);
	return r;
}
