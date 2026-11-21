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

#include "libm.h"

/* Correctly rounded long double ln, log2, log10, atanh, sin and cos: the
 * value is computed as a sum of two long doubles (about 2^-115 relative)
 * and rounded once.  Only for the finite arguments the callers pass. */

typedef struct { long double h, l; } dd;

static dd qsum(long double a, long double b)   /* |a| >= |b| */
{
	dd r;
	long double s = a + b;
	r.l = b - (s - a);
	r.h = s;
	return r;
}

static dd tsum(long double a, long double b)
{
	dd r;
	__two_sum(a, b, &r.h, &r.l);
	return r;
}

static dd dneg(dd a)
{
	a.h = -a.h;
	a.l = -a.l;
	return a;
}

static dd dadd(dd a, dd b)
{
	long double s1, s2, t1, t2;
	dd r;
	__two_sum(a.h, b.h, &s1, &s2);
	__two_sum(a.l, b.l, &t1, &t2);
	s2 += t1;
	r = tsum(s1, s2);
	r.l += t2;
	return tsum(r.h, r.l);
}

static dd dmul(dd a, dd b)
{
	dd r;
	__two_prod(a.h, b.h, &r.h, &r.l);
	r.l += a.h * b.l + a.l * b.h;
	return qsum(r.h, r.l);
}

static dd dmuld(dd a, long double b)
{
	dd r;
	__two_prod(a.h, b, &r.h, &r.l);
	r.l += a.l * b;
	return qsum(r.h, r.l);
}

static dd ddiv(dd a, dd b)
{
	long double q1, q2, q3;
	dd r;
	q1 = a.h / b.h;
	r = dadd(a, dneg(dmuld(b, q1)));
	q2 = r.h / b.h;
	r = dadd(r, dneg(dmuld(b, q2)));
	q3 = r.h / b.h;
	r = qsum(q1, q2);
	return dadd(r, tsum(q3, 0));
}

static long double p2(int e)   /* 2^e */
{
	union __ldbits u = {0};
	u.i.m = 0x8000000000000000ULL;
	u.i.se = (uint16_t)(e + 16383);
	return u.f;
}

#define C(a, b) {a, b}
static const dd LN2 = C(0xb17217f7d1cf79acp-64L, -0xd871319ff0342543p-130L);
static const dd LOG2E = C(0xb8aa3b295c17f0bcp-63L, -0x82f0025f2dc582eep-128L);
static const dd LOG10E = C(0xde5bd8a937287195p-65L, 0xd56eaabeb4cf70c9p-131L);
static const long double PIO2[3] = {0xc90fdaa22168c235p-63L, -0xece675d1fc8f8cbbp-129L, -0xb7ed8fbbacc19c60p-194L};
#undef C

static int tiny(dd t, dd sum)
{
	return __ld_abs(t.h) <= __ld_abs(sum.h) * 0x1p-127L;
}

/* 2 atanh(z) = ln((1 + z)/(1 - z)) = 2 (z + z^3/3 + z^5/5 + ...) */
static dd atanh_series(dd z)
{
	dd z2 = dmul(z, z), term = z, sum = z, q;
	long double n;
	if (z.h == 0)
		return z;
	for (n = 3; n < 400; n += 2) {
		term = dmul(term, z2);
		q = ddiv(term, tsum(n, 0));
		sum = dadd(sum, q);
		if (tiny(q, sum))
			break;
	}
	return sum;
}

/* ln of m = hi + lo in [sqrt(1/2), sqrt(2)] */
static dd ln_m(dd m)
{
	dd one = tsum(1, 0), z = ddiv(dadd(m, dneg(one)), dadd(m, one));
	dd s = atanh_series(z);
	s.h *= 2;
	s.l *= 2;
	return s;
}

/* ln of a positive finite dd, k scaled out */
static dd ln_dd(dd x)
{
	int k, e;
	dd r;
	union __ldbits u = {x.h};
	if ((u.i.se & 0x7fff) == 0) {          /* subnormal */
		x.h *= 0x1p64L;
		x.l *= 0x1p64L;
		u.f = x.h;
		k = -64;
	} else
		k = 0;
	e = (u.i.se & 0x7fff) - 16383;
	k += e;
	x.h *= p2(-(e / 2));
	x.h *= p2(-(e - e / 2));
	x.l *= p2(-(e / 2));
	x.l *= p2(-(e - e / 2));
	if (x.h > 0x1.6A09E667F3BCC90p0L) {
		x.h *= 0.5L;
		x.l *= 0.5L;
		k++;
	}
	r = ln_m(x);
	return dadd(r, dmuld(LN2, (long double)k));
}

long double __ddl_logl(long double x)    /* ln x, x > 0 finite */
{
	dd r = ln_dd(tsum(x, 0));
	return r.h;
}

long double __ddl_log2l(long double x)
{
	dd r = dmul(ln_dd(tsum(x, 0)), LOG2E);
	return r.h;
}

long double __ddl_log10l(long double x)
{
	dd r = dmul(ln_dd(tsum(x, 0)), LOG10E);
	return r.h;
}

/* atanh x for 0 < |x| < 1, |x| >= 2^-32 */
long double __ddl_atanhl(long double x)
{
	long double ax = __ld_abs(x);
	dd r, s;
	if (ax < 0.2L)
		r = atanh_series(tsum(ax, 0));
	else {
		s = ddiv(tsum(1, ax), tsum(1, -ax));
		r = ln_dd(s);
		r.h *= 0.5L;
		r.l *= 0.5L;
	}
	return x < 0 ? -r.h : r.h;
}

/* sin r or cos r, |r| <= about pi/4 */
static dd sincos_r(dd r, int wantsin)
{
	dd r2 = dmul(r, r), term, sum;
	long double n;
	if (wantsin) {
		term = r;
		n = 2;
	} else {
		term = tsum(1, 0);
		n = 1;
	}
	sum = term;
	for (; n < 200; n += 2) {
		term = ddiv(dmul(term, r2), tsum(-(n * (n + 1)), 0));
		sum = dadd(sum, term);
		if (tiny(term, sum))
			break;
	}
	return sum;
}

/* x = q pi/2 + r, |x| < 2^20; returns q, r as a dd (pi/2 in 192 bits) */
static long q_rem(long double x, dd *r)
{
	long q = (long)(x * 0xa2f9836e4e44152ap-64L + (x < 0 ? -0.5L : 0.5L));
	long double n = (long double)q, a, b;
	dd s = tsum(x, 0);
	int i;
	for (i = 0; i < 3; i++) {
		__two_prod(n, PIO2[i], &a, &b);
		s = dadd(s, dneg(tsum(a, b)));
	}
	*r = s;
	return q;
}

/* sin x and cos x, |x| < 2^20; the result rounded once */
long double __ddl_sinl(long double x)
{
	dd r, v;
	long q = q_rem(x, &r);
	switch (q & 3) {
	case 0: v = sincos_r(r, 1); break;
	case 1: v = sincos_r(r, 0); break;
	case 2: v = dneg(sincos_r(r, 1)); break;
	default: v = dneg(sincos_r(r, 0)); break;
	}
	return v.h;
}

long double __ddl_cosl(long double x)
{
	dd r, v;
	long q = q_rem(x, &r);
	switch (q & 3) {
	case 0: v = sincos_r(r, 0); break;
	case 1: v = dneg(sincos_r(r, 1)); break;
	case 2: v = dneg(sincos_r(r, 0)); break;
	default: v = sincos_r(r, 1); break;
	}
	return v.h;
}

/* The complex functions on finite arguments away from zero, the poles and
 * the branch points, as dd values rounded once; the callers use the
 * ordinary code where these return 0. */

static dd dsub(dd a, dd b) { return dadd(a, dneg(b)); }
static dd dsc(dd a, long double s) { a.h *= s; a.l *= s; return a; }
static dd dabs(dd a) { return a.h < 0 ? dneg(a) : a; }
static dd one_dd(void) { return tsum(1, 0); }

static int inreg(long double v, long double hi)
{
	v = __ld_abs(v);
	return v >= 0x1p-64L && v <= hi;
}

static int inregz(long double v, long double hi)    /* also 0 */
{
	return v == 0 || inreg(v, hi);
}

static dd sqrt_dd(dd a)    /* a > 0 */
{
	long double s0 = sqrtl(a.h), p, q;
	dd r;
	__two_prod(s0, s0, &p, &q);
	r = dadd(a, dneg(tsum(p, q)));
	return tsum(s0, r.h / (2 * s0));
}

static dd exp_dd(dd a)    /* |a| < 1000 */
{
	long k = (long)(a.h * 0xb8aa3b295c17f0bcp-63L + (a.h < 0 ? -0.5L : 0.5L));
	dd r = dsub(a, dmuld(LN2, (long double)k)), term = one_dd(), sum = term;
	long double n, s = p2((int)k);
	for (n = 1; n < 100; n++) {
		term = ddiv(dmul(term, r), tsum(n, 0));
		sum = dadd(sum, term);
		if (tiny(term, sum))
			break;
	}
	return dsc(sum, s);
}

static void sinhcosh_dd(dd x, dd *sh, dd *ch)
{
	dd x2, ts, tc, e, ei;
	long double n;
	if (__ld_abs(x.h) < 1) {
		x2 = dmul(x, x);
		ts = *sh = x;
		tc = *ch = one_dd();
		for (n = 1; n < 100; n += 2) {
			tc = ddiv(dmul(tc, x2), tsum(n * (n + 1), 0));
			ts = ddiv(dmul(ts, x2), tsum((n + 1) * (n + 2), 0));
			*ch = dadd(*ch, tc);
			*sh = dadd(*sh, ts);
			if (tiny(tc, *ch) && tiny(ts, *sh))
				break;
		}
		return;
	}
	e = exp_dd(x);
	ei = ddiv(one_dd(), e);
	*sh = dsc(dsub(e, ei), 0.5L);
	*ch = dsc(dadd(e, ei), 0.5L);
}

static long q_remd(dd x, dd *r)
{
	long q = (long)(x.h * 0xa2f9836e4e44152ap-64L + (x.h < 0 ? -0.5L : 0.5L));
	long double n = (long double)q, a, b;
	dd s = x;
	int i;
	for (i = 0; i < 3; i++) {
		__two_prod(n, PIO2[i], &a, &b);
		s = dadd(s, dneg(tsum(a, b)));
	}
	*r = s;
	return q;
}

static void sincos_dd(dd x, dd *s, dd *c)    /* |x| < 2^20 */
{
	dd r, sn, cs;
	long q = q_remd(x, &r);
	sn = sincos_r(r, 1);
	cs = sincos_r(r, 0);
	switch (q & 3) {
	case 0: *s = sn; *c = cs; break;
	case 1: *s = cs; *c = dneg(sn); break;
	case 2: *s = dneg(sn); *c = dneg(cs); break;
	default: *s = dneg(cs); *c = sn; break;
	}
}

static dd atan2_dd(dd y, dd x)
{
	dd th = tsum(atan2l(y.h, x.h), 0), s, c;
	sincos_dd(th, &s, &c);
	return dadd(th, ddiv(dsub(dmul(y, c), dmul(x, s)), dadd(dmul(x, c), dmul(y, s))));
}

static dd atanh_dd(dd t)    /* |t| < 1 */
{
	dd a = dabs(t), r;
	if (a.h < 0.2L)
		return atanh_series(t);
	r = dsc(ln_dd(ddiv(dadd(one_dd(), a), dsub(one_dd(), a))), 0.5L);
	return t.h < 0 ? dneg(r) : r;
}

static dd asinh_dd(dd t)
{
	dd a = dabs(t), r = ln_dd(dadd(a, sqrt_dd(dadd(dmul(a, a), one_dd()))));
	return t.h < 0 ? dneg(r) : r;
}

static void csqrt_dd(dd u, dd v, dd *re, dd *im)
{
	dd m = sqrt_dd(dadd(dmul(u, u), dmul(v, v))), t;
	if (u.h >= 0) {
		*re = sqrt_dd(dsc(dadd(m, u), 0.5L));
		*im = ddiv(v, dsc(*re, 2));
	} else {
		t = sqrt_dd(dsc(dsub(m, u), 0.5L));
		*im = v.h < 0 ? dneg(t) : t;
		*re = ddiv(dabs(v), dsc(t, 2));
	}
}

int __ddl_cexpl(long double x, long double y, long double *re, long double *im)
{
	dd e, s, c;
	if (!inregz(x, 1000) || !inreg(y, 0x1p19L))
		return 0;
	sincos_dd(tsum(y, 0), &s, &c);
	e = exp_dd(tsum(x, 0));
	*re = dmul(e, c).h;
	*im = dmul(e, s).h;
	return 1;
}

/* sinh and cosh of x + iy: wantsin selects csinh, else ccosh */
static int csc_h(long double x, long double y, int wantsin, long double *re, long double *im)
{
	dd sh, ch, s, c;
	if (!inregz(x, 1000) || !inreg(y, 0x1p19L))
		return 0;
	sinhcosh_dd(tsum(x, 0), &sh, &ch);
	sincos_dd(tsum(y, 0), &s, &c);
	if (x == 0) {
		*re = wantsin ? x * c.h : c.h;
		*im = wantsin ? s.h : x * s.h;
		return 1;
	}
	if (wantsin) {
		*re = dmul(sh, c).h;
		*im = dmul(ch, s).h;
	} else {
		*re = dmul(ch, c).h;
		*im = dmul(sh, s).h;
	}
	return 1;
}

int __ddl_csinhl(long double x, long double y, long double *re, long double *im)
{
	return csc_h(x, y, 1, re, im);
}

int __ddl_ccoshl(long double x, long double y, long double *re, long double *im)
{
	return csc_h(x, y, 0, re, im);
}

/* tanh z = (sinh 2x + i sin 2y) / (cosh 2x + cos 2y) */
int __ddl_ctanhl(long double x, long double y, long double *re, long double *im)
{
	dd sh, ch, s, c, d;
	if (!inregz(x, 500) || !inreg(y, 0x1p18L))
		return 0;
	sinhcosh_dd(tsum(2 * x, 0), &sh, &ch);
	sincos_dd(tsum(2 * y, 0), &s, &c);
	d = dadd(ch, c);
	if (d.h < 0x1p-24L)
		return 0;
	*re = x == 0 ? x : ddiv(sh, d).h;
	*im = ddiv(s, d).h;
	return 1;
}

/* atanh z = atanh(2x / (1 + |z|^2)) / 2 + i atan2(2y, 1 - |z|^2) / 2 */
int __ddl_catanhl(long double x, long double y, long double *re, long double *im)
{
	dd m, t, g;
	if (y == 0 && inreg(x, 1) && __ld_abs(x) < 1 && 1 - __ld_abs(x) > 0x1p-24L) {
		*re = atanh_dd(tsum(x, 0)).h;
		*im = y;
		return 1;
	}
	if (!inregz(x, 0x1p30L) || !inreg(y, 0x1p30L))
		return 0;
	m = dadd(dmul(tsum(x, 0), tsum(x, 0)), dmul(tsum(y, 0), tsum(y, 0)));
	t = ddiv(tsum(2 * x, 0), dadd(one_dd(), m));
	g = dsub(one_dd(), dabs(t));
	if (g.h < 0x1p-24L)
		return 0;
	*re = x == 0 ? x : dsc(atanh_dd(t), 0.5L).h;
	*im = dsc(atan2_dd(tsum(2 * y, 0), dsub(one_dd(), m)), 0.5L).h;
	return 1;
}

/* asinh z = asinh(Im(conj(sqrt(1-w)) sqrt(1+w))) ..., w = -y + ix (Kahan) */
int __ddl_casinhl(long double x, long double y, long double *re, long double *im)
{
	dd r1, i1, r2, i2, eta, xi;
	if (!inreg(x, 0x1p30L) || !inreg(y, 0x1p30L))
		return 0;
	csqrt_dd(tsum(1, y), tsum(-x, 0), &r1, &i1);
	csqrt_dd(tsum(1, -y), tsum(x, 0), &r2, &i2);
	xi = atan2_dd(tsum(-y, 0), dsub(dmul(r1, r2), dmul(i1, i2)));
	eta = dsub(dmul(r1, i2), dmul(i1, r2));
	if (__ld_abs(eta.h) < 0x1p-24L)
		return 0;
	*re = asinh_dd(eta).h;
	*im = dneg(xi).h;
	return 1;
}

/* acosh z = asinh(Re(conj(sqrt(z-1)) sqrt(z+1))) + 2i atan2(Im sqrt(z-1), Re sqrt(z+1)) */
int __ddl_cacoshl(long double x, long double y, long double *re, long double *im)
{
	dd r1, i1, r2, i2, t;
	if (!inregz(x, 0x1p30L) || !inreg(y, 0x1p30L))
		return 0;
	csqrt_dd(tsum(x, -1), tsum(y, 0), &r1, &i1);
	csqrt_dd(tsum(x, 1), tsum(y, 0), &r2, &i2);
	t = dadd(dmul(r1, r2), dmul(i1, i2));
	if (__ld_abs(t.h) < 0x1p-24L)
		return 0;
	*re = asinh_dd(t).h;
	*im = dsc(atan2_dd(i1, r2), 2).h;
	return 1;
}

/* z^w = exp(w log z) */
int __ddl_cpowl(long double x, long double y, long double c, long double d,
                long double *re, long double *im)
{
	dd m, lr, th, tr, ti, e, s, co, dc = tsum(c, 0), dd_ = tsum(d, 0), yy = {y, 0};
	if (!inregz(x, 0x1p30L) || !inregz(y, 0x1p30L) || (x == 0 && y == 0) || __ld_abs(c) > 0x1p30L || __ld_abs(d) > 0x1p30L
	    || c != c || d != d)
		return 0;
	m = dadd(dmul(tsum(x, 0), tsum(x, 0)), dmul(yy, yy));
	if (__ld_abs(dsub(m, one_dd()).h) < 0x1p-30L)
		return 0;
	lr = dsc(ln_dd(m), 0.5L);
	th = atan2_dd(yy, tsum(x, 0));
	tr = dsub(dmul(dc, lr), dmul(dd_, th));
	ti = dadd(dmul(dc, th), dmul(dd_, lr));
	if (__ld_abs(tr.h) > 1000 || __ld_abs(ti.h) > 0x1p19L)
		return 0;
	sincos_dd(ti, &s, &co);
	e = exp_dd(tr);
	*re = dmul(e, co).h;
	*im = dmul(e, s).h;
	return 1;
}
