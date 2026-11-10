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

/* fma(x, y, z) = x y + z rounded once, in integer arithmetic: the
 * 128-bit product and z, aligned in 192 bits (with a sticky bit), added,
 * then rounded to the precision of the type in the current rounding
 * mode, subnormal results included.  One routine serves all three types
 * (the operands are converted to long double, which is exact). */
#include "libm.h"
#include <fenv.h>

typedef struct { uint64_t w[3]; } u192;    /* w[0] most significant */

static int clz64(uint64_t x)
{
	int n = 0;
	if (!x)
		return 64;
	while (!(x >> 63)) {
		x <<= 1;
		n++;
	}
	return n;
}

static void mul64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
	uint64_t a0 = (uint32_t)a, a1 = a >> 32, b0 = (uint32_t)b, b1 = b >> 32;
	uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
	*lo = (mid << 32) | (uint32_t)p00;
	*hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/* w >>= n with the bits shifted out ORed into bit 0 */
static void shr(u192 *a, int n)
{
	uint64_t s = 0;
	int i;
	if (n <= 0)
		return;
	if (n >= 192) {
		s = a->w[0] | a->w[1] | a->w[2];
		a->w[0] = a->w[1] = 0;
		a->w[2] = s != 0;
		return;
	}
	while (n >= 64) {
		s |= a->w[2];
		a->w[2] = a->w[1]; a->w[1] = a->w[0]; a->w[0] = 0;
		n -= 64;
	}
	if (n) {
		s |= a->w[2] << (64 - n);
		for (i = 2; i > 0; i--)
			a->w[i] = a->w[i] >> n | a->w[i - 1] << (64 - n);
		a->w[0] >>= n;
	}
	a->w[2] |= s != 0;
}

static void shl(u192 *a, int n)
{
	int i;
	while (n >= 64) {
		a->w[0] = a->w[1]; a->w[1] = a->w[2]; a->w[2] = 0;
		n -= 64;
	}
	if (n) {
		for (i = 0; i < 2; i++)
			a->w[i] = a->w[i] << n | a->w[i + 1] >> (64 - n);
		a->w[2] <<= n;
	}
}

static int cmp(const u192 *a, const u192 *b)
{
	int i;
	for (i = 0; i < 3; i++)
		if (a->w[i] != b->w[i])
			return a->w[i] < b->w[i] ? -1 : 1;
	return 0;
}

static int norm(u192 *a)          /* shift left until bit 191 is set */
{
	int n = 0, k;
	while (!a->w[0]) {
		shl(a, 64);
		n += 64;
	}
	k = clz64(a->w[0]);
	shl(a, k);
	return n + k;
}

/* bit i of a (0 = least significant) */
static int bit(const u192 *a, int i)
{
	return (int)(a->w[2 - i / 64] >> (i % 64) & 1);
}

/* any of bits 0 .. n-1 set */
static int below(const u192 *a, int n)
{
	int i;
	for (i = 0; i < n && i < 192; i++)
		if (bit(a, i))
			return 1;
	return 0;
}

static void unpack(long double x, int *neg, uint64_t *m, int *e)
{
	union __ldbits u = {x};
	int be = u.i.se & 0x7fff;
	*neg = u.i.se >> 15;
	*m = u.i.m;
	*e = (be ? be : 1) - 16383 - 63;
}

/* p bits of precision; the normal exponents are emin..emax */
static long double fma_core(long double x, long double y, long double z, int p, int emin, int emax)
{
	int nx, ny, nz, ex, ey, ez, neg, eA, eB, M, q, s, rb, st, inc, mode;
	uint64_t mx, my, mz, kept;
	u192 A, B;
	long double r;

	if (x != x || y != y || z != z || __ld_nonfinite(x) || __ld_nonfinite(y) || __ld_nonfinite(z) || x == 0 || y == 0) {
		/* a domain error: an infinity times zero (also with a NaN z,
		   where POSIX says "may"), or infinities of opposite signs
		   added; NaN operands otherwise give none */
		int xi = __ld_nonfinite(x) && x == x, yi = __ld_nonfinite(y) && y == y;
		if ((xi && y == 0) || (yi && x == 0) ||
		    ((xi || yi) && x == x && y == y && __ld_nonfinite(z) && z == z &&
		     (x * y > 0) != (z > 0)))
			errno = EDOM;
		return x * y + z;           /* exact: NaN, infinite, or z +- 0 */
	}
	unpack(x, &nx, &mx, &ex);
	unpack(y, &ny, &my, &ey);
	neg = nx ^ ny;
	A.w[2] = 0;
	mul64(mx, my, &A.w[0], &A.w[1]);
	eA = ex + ey - 64;               /* value = A 2^eA */
	eA -= norm(&A);
	if (z != 0) {
		unpack(z, &nz, &mz, &ez);
		B.w[0] = mz; B.w[1] = B.w[2] = 0;
		eB = ez - 128;
		eB -= norm(&B);
		if (eB > eA || (eB == eA && cmp(&B, &A) > 0)) {
			u192 t = A; int te = eA, tn = neg;
			A = B; eA = eB; neg = nz;
			B = t; eB = te; nz = tn;
		}
		shr(&B, eA - eB);
		if (nz == neg) {             /* A + B */
			uint64_t c = 0, t;
			int i;
			for (i = 2; i >= 0; i--) {
				t = A.w[i] + B.w[i];
				uint64_t c1 = t < A.w[i];
				A.w[i] = t + c;
				c = c1 | (A.w[i] < t);
			}
			if (c) {
				shr(&A, 1);
				A.w[0] |= 1ULL << 63;
				eA++;
			}
		} else {                     /* A - B, A >= B */
			uint64_t b = 0, t;
			int i;
			for (i = 2; i >= 0; i--) {
				t = A.w[i] - B.w[i];
				uint64_t b1 = A.w[i] < B.w[i];
				A.w[i] = t - b;
				b = b1 | (t < b);
			}
			if (!(A.w[0] | A.w[1] | A.w[2]))
				return fegetround() == FE_DOWNWARD ? -0.0L : 0.0L;
			eA -= norm(&A);
		}
	}
	/* round A 2^eA to q bits */
	M = eA + 191;                       /* exponent of the leading bit */
	q = M < emin ? p - (emin - M) : p;
	s = 192 - q;                        /* bits dropped */
	if (s >= 192) {
		kept = 0;
		rb = s == 192 ? bit(&A, 191) : 0;
		st = s == 192 ? below(&A, 191) : 1;
	} else {
		u192 t = A;
		shr(&t, s);
		kept = t.w[2] & ~1ULL;              /* bit 0 may hold sticky */
		kept |= bit(&A, s);
		rb = bit(&A, s - 1);
		st = below(&A, s - 1);
	}
	mode = fegetround();
	switch (mode) {
	case FE_TOWARDZERO: inc = 0; break;
	case FE_UPWARD: inc = (rb | st) && !neg; break;
	case FE_DOWNWARD: inc = (rb | st) && neg; break;
	default: inc = rb && (st || (kept & 1)); break;
	}
	s = M - q + 1;                       /* exponent of the last kept bit */
	if (inc) {
		kept++;
		if (kept == 0) {             /* 2^64 */
			kept = 1ULL << 63;
			s++;
		}
	}
	if (s + 63 - clz64(kept) > emax && kept) {
		feraiseexcept(FE_OVERFLOW | FE_INEXACT);
		if (mode == FE_TOWARDZERO || (mode == FE_UPWARD && neg) || (mode == FE_DOWNWARD && !neg))
			r = __x87_scale(__x87_scale(1, p) - 1, emax - p + 1);   /* largest finite */
		else
			r = HUGE_VALL;
		return neg ? -r : r;
	}
	if (rb | st)
		feraiseexcept(M < emin ? FE_UNDERFLOW | FE_INEXACT : FE_INEXACT);
	r = __x87_scale((long double)kept, s);
	return neg ? -r : r;
}

long double fmal(long double x, long double y, long double z)
{
	return fma_core(x, y, z, 64, -16382, 16383);
}

double fma(double x, double y, double z)
{
	return fma_core(x, y, z, 53, -1022, 1023);
}

float fmaf(float x, float y, float z)
{
	return fma_core(x, y, z, 24, -126, 127);
}
