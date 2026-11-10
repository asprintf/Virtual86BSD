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

/* Argument reduction for the trigonometric functions: x = n pi/2 + r.
 *
 * |x| < 2^23: Cody-Waite with pi/2 in 40-bit pieces, so that n * piece
 * is exact and the subtractions cancel exactly.  Larger x: Payne-Hanek,
 * x * 2/pi computed exactly modulo 4 from the bits of 2/pi (twoopi.h)
 * with 256 bits after the binary point.
 */
#include "libm.h"
#include "twoopi.h"

#define P1 0x1.921FB54442p0L                        /* 40 bits */
#define P2 0x0.0000000000D18469898Cp0L              /* next 40 */
#define P3 0x0.00000000000000000000C51701B839p0L    /* next 40 */
#define P4 0x0.000000000000000000000000000000A252049C1114CF98E804177D4C76p0L

/* bits [i, i + 32) of 2/pi after the point (i counted from 0); 0 before */
static uint32_t word(int i)
{
	int w, s;
	uint64_t v;
	if (i <= -32)
		return 0;
	if (i < 0)
		return __twoopi[0] >> -i;
	w = i >> 5; s = i & 31;
	v = (uint64_t)__twoopi[w] << 32 | __twoopi[w + 1];
	return (uint32_t)(v >> (32 - s));
}

static int payne_hanek(long double x, long double *r)
{
	union __ldbits u = {x};
	int e = (u.i.se & 0x7fff) - 16383;   /* x = m * 2^(e - 63), m 64 bits */
	uint32_t m[2], b[8], p[10];
	uint64_t t;
	int i, j, i0, n, neg = u.i.se >> 15;
	long double f;

	/* bit j of 2/pi (0-based, weight 2^-(j+1)) contributes a multiple of
	 * 4 to x * 2/pi when j < e - 65: the window of 256 bits starts there */
	i0 = e - 65;
	for (i = 0; i < 8; i++)
		b[i] = word(i0 + 32 * i);         /* b[0] most significant */
	m[0] = (uint32_t)(u.i.m >> 32); m[1] = (uint32_t)u.i.m;
	for (i = 0; i < 10; i++)
		p[i] = 0;
	/* p (320 bits, p[0] most significant) = m * b */
	for (i = 1; i >= 0; i--) {
		t = 0;
		for (j = 7; j >= 0; j--) {
			t += (uint64_t)m[i] * b[j] + p[i + j + 1];
			p[i + j + 1] = (uint32_t)t;
			t >>= 32;
		}
		p[i] = (uint32_t)t;
	}
	/* value = p * 2^(e - 63 - (i0 + 256)) = p * 2^-254: the low 2 bits of
	 * the integer part are the top 2 bits of p[2]; the fraction follows */
	n = p[2] >> 30;
	{
		uint64_t hi = ((uint64_t)p[2] << 32 | p[3]) << 2 | p[4] >> 30;
		uint64_t lo = ((uint64_t)p[4] << 32 | p[5]) << 2 | p[6] >> 30;
		if (hi >> 63) {              /* fraction >= 1/2: f - 1 */
			n = (n + 1) & 3;
			hi = ~hi; lo = ~lo;  /* 1 - f, to within 2^-128 */
			f = -((long double)hi + (long double)lo * 0x1p-64L) * 0x1p-64L;
		} else
			f = ((long double)hi + (long double)lo * 0x1p-64L) * 0x1p-64L;
	}
	/* r = f * pi/2, with pi/2 as hi + lo */
	*r = f * 0x1.921FB54442D18468p0L + f * 0x1.898CC51701B839A252049Cp-64L;
	if (neg) {
		*r = -*r;
		n = -n & 3;
	}
	return n;
}

int __rem_pio2l(long double x, long double *r)
{
	long double k, ax = __ld_abs(x);
	if (ax <= 0x1.921FB54442D18469p-1L) {      /* |x| <= pi/4 */
		*r = x;
		return 0;
	}
	if (ax < 0x1p23L) {
		k = __x87_rint(x * 0x0.A2F9836E4E441529FC2757D1F534DDC0p0L);   /* x * 2/pi */
		*r = (((x - k * P1) - k * P2) - k * P3) - k * P4;
		return (int)k & 3;
	}
	return payne_hanek(x, r);
}

/* sin(pi x) and cos(pi x), with x reduced exactly modulo 2 */
static long double reduce(long double x, int *n)
{
	long double k2;
	if (__ld_abs(x) >= 0x1p62L) {      /* an even integer */
		*n = 0;
		return 0;
	}
	/* x = k2/2 + t, |t| <= 1/4, k2 = rint(2x); n = k2 mod 4 */
	k2 = __x87_rint(2 * x);
	*n = (int)(k2 - 4 * __x87_rint(k2 / 4)) & 3;
	return x - k2 / 2;
}

long double __sinpil(long double x)
{
	int n;
	long double t = reduce(x, &n), r;
	switch (n) {
	case 0: r = __x87_sin(__PI * t); break;
	case 1: r = __x87_cos(__PI * t); break;
	case 2: r = -__x87_sin(__PI * t); break;
	default: r = -__x87_cos(__PI * t); break;
	}
	return r == 0 ? r * x : r;   /* sign of zero: that of x */
}

long double __cospil(long double x)
{
	int n;
	long double t = reduce(x, &n);
	switch (n) {
	case 0: return __x87_cos(__PI * t);
	case 1: return -__x87_sin(__PI * t);
	case 2: return -__x87_cos(__PI * t);
	default: return __x87_sin(__PI * t);
	}
}
