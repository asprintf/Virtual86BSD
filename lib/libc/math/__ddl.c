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
