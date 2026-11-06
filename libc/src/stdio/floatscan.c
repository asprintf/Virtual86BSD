/* __floatscan: read a floating-point number (strtod syntax) from a stream
 * and convert it correctly rounded (to nearest, ties to even) to float,
 * double or long double precision.
 *
 * The decimal value D * 10^E (or hexadecimal M * 2^E) is converted
 * exactly with big integers: with A/B = |value| (B a power of ten or of
 * two), the binary exponent te of the value is found by comparison, and
 * the mantissa bits plus a rounding bit are obtained by long division;
 * a non-zero remainder is the sticky bit.  Up to MAXDIG significant
 * digits are kept; further non-zero digits are folded into a final 1 so
 * that ties are still decided correctly.
 */
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "stdio_impl.h"
#include "locale_impl.h"
#include <float.h>
#include <math.h>

/* too large: infinity, with FE_OVERFLOW (not the FE_DIVBYZERO of 1/0) */
static long double overflow(void)
{
	volatile long double big = LDBL_MAX;
	return big * 2;
}

#define MAXDIG 12000

struct big { uint32_t *d; int n; };

static void big_mul_add(struct big *b, uint32_t m, uint32_t a)
{
	uint64_t carry = a;
	int i;
	for (i = 0; i < b->n; i++) {
		uint64_t t = (uint64_t)b->d[i] * m + carry;
		b->d[i] = (uint32_t)t;
		carry = t >> 32;
	}
	if (carry) b->d[b->n++] = (uint32_t)carry;
}

static void big_shl(struct big *b, int k)
{
	int w = k / 32, s = k % 32, i;
	if (!b->n || !k) return;
	if (s) {
		uint32_t carry = 0;
		for (i = 0; i < b->n; i++) {
			uint32_t v = b->d[i];
			b->d[i] = v << s | carry;
			carry = v >> (32 - s);
		}
		if (carry) b->d[b->n++] = carry;
	}
	if (w) {
		for (i = b->n - 1; i >= 0; i--) b->d[i + w] = b->d[i];
		for (i = 0; i < w; i++) b->d[i] = 0;
		b->n += w;
	}
}

static int big_cmp(const struct big *a, const struct big *b)
{
	int i;
	if (a->n != b->n) return a->n < b->n ? -1 : 1;
	for (i = a->n - 1; i >= 0; i--)
		if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
	return 0;
}

static void big_sub(struct big *a, const struct big *b)   /* a >= b */
{
	uint64_t borrow = 0;
	int i;
	for (i = 0; i < a->n; i++) {
		uint64_t t = (uint64_t)a->d[i] - (i < b->n ? b->d[i] : 0) - borrow;
		a->d[i] = (uint32_t)t;
		borrow = t >> 63;
	}
	while (a->n && !a->d[a->n - 1]) a->n--;
}

static int big_bits(const struct big *b)
{
	uint32_t top;
	int n;
	if (!b->n) return 0;
	top = b->d[b->n - 1];
	for (n = 0; top; top >>= 1) n++;
	return (b->n - 1) * 32 + n;
}

/* An exact long double from N * 2^lsb (N < 2^64, the result representable). */
static long double make_ld(uint64_t N, int lsb)
{
	union { long double f; struct { uint64_t m; uint16_t se; } i; } u;
	int top, biased;
	memset(&u, 0, sizeof u);
	if (!N) return 0;
	for (top = 63; !(N >> top); top--);
	N <<= 63 - top;
	biased = lsb + top + 16383;
	if (biased <= 0) { N >>= 1 - biased; biased = 0; }
	u.i.m = N;
	u.i.se = (uint16_t)biased;
	return u.f;
}

/* Round A/B * 2^bexp (A, B > 0) to P bits with exponent range [emin, emax]. */
static long double convert(struct big *A, struct big *B, long bexp, int P, int emin, int emax, uint32_t *tmp)
{
	long k, lsb, nbits;
	int i, roundbit, sticky;
	uint64_t N = 0;
	struct big S;
	(void)tmp;

	/* 2^k <= A/B < 2^(k+1) */
	k = (long)big_bits(A) - big_bits(B);
	if (k >= 0) big_shl(B, (int)k); else big_shl(A, (int)-k);
	if (big_cmp(A, B) < 0) { k--; big_shl(A, 1); }
	/* now B' = B * 2^k (relative), A >= B, A < 2B */
	k += bexp;
	if (k > emax) { errno = ERANGE; return overflow(); }
	lsb = k - (P - 1);
	if (lsb < emin - (P - 1)) lsb = emin - (P - 1);
	nbits = k - lsb + 1;                /* mantissa bits to produce, <= P */
	if (nbits < 0) { errno = ERANGE; return 0; }   /* below half the smallest subnormal */

	/* long division: nbits mantissa bits, then the rounding bit */
	S = *A;   /* the remainder, in A's storage */
	for (i = 0; i < nbits; i++) {
		N <<= 1;
		if (big_cmp(&S, B) >= 0) { big_sub(&S, B); N |= 1; }
		big_shl(&S, 1);
	}
	roundbit = big_cmp(&S, B) >= 0;
	if (roundbit) big_sub(&S, B);
	sticky = S.n != 0;
	if (roundbit && (sticky || (N & 1))) {
		N++;
		/* a carry out of the mantissa (for P == 64 it wraps to 0) */
		if (nbits == P && (P == 64 ? N == 0 : N >> P)) {
			N = P == 64 ? 1ULL << 63 : N >> 1;
			lsb++;
			if (lsb + P - 1 > emax) { errno = ERANGE; return overflow(); }
		}
	}
	/* underflow: an inexact result that is still tiny after rounding */
	if (roundbit || sticky) {
		int top = 63;
		while (top >= 0 && !(N >> top)) top--;
		if (top < 0 || lsb + top < emin) errno = ERANGE;
	}
	return make_ld(N, (int)lsb);
}

static int lower(int c) { return c | 32; }

static long double qnan(void)
{
	union { long double f; struct { uint64_t m; uint16_t se; } i; } u;
	memset(&u, 0, sizeof u);
	u.i.m = 0xc000000000000000ULL;
	u.i.se = 0x7fff;
	return u.f;
}

long double __floatscan(FILE *f, int prec, int pok)
{
	static const int Ps[] = { 24, 53, 64 }, emins[] = { -126, -1022, -16382 }, emaxs[] = { 127, 1023, 16383 };
	int P = Ps[prec], emin = emins[prec], emax = emaxs[prec];
	int c, neg = 0, i;
	char digits[MAXDIG + 1];
	int nd = 0, lost = 0, gotdig = 0, gotrad = 0;
	long long e10 = 0, ex = 0;
	long double r;
	/* the radix character of the locale (a single byte; else '.') */
	const char *dp = __lc_str(CURRENT_LOCALE->cat[LC_NUMERIC], 0);
	int rad = dp[0] && !dp[1] ? (unsigned char)dp[0] : '.';

	c = shgetc(f);
	while (c == ' ' || (unsigned)c - '\t' < 5) c = shgetc(f);
	if (c == '+' || c == '-') { neg = c == '-'; c = shgetc(f); }

	/* inf, infinity */
	for (i = 0; i < 8 && lower(c) == "infinity"[i]; i++) if (i < 7) c = shgetc(f);
	if (i == 3 || i == 8 || (i > 3 && pok)) {
		if (i != 8) {
			shunget(f);
			if (pok) for (; i > 3; i--) shunget(f);
		}
		return neg ? -HUGE_VALL : HUGE_VALL;   /* "inf": no exception */
	}
	if (!i) {
		/* nan, nan(n-char-sequence) */
		for (i = 0; i < 3 && lower(c) == "nan"[i]; i++) if (i < 2) c = shgetc(f);
		if (i == 3) {
			int n = 0;
			c = shgetc(f);
			if (c != '(') { shunget(f); return neg ? -qnan() : qnan(); }
			for (;;) {
				c = shgetc(f);
				n++;
				if ((unsigned)c - '0' < 10 || (unsigned)(c | 32) - 'a' < 26 || c == '_') continue;
				if (c == ')') return neg ? -qnan() : qnan();
				shunget(f);
				if (!pok) { shlim(f, 0); return 0; }
				while (n--) shunget(f);
				return neg ? -qnan() : qnan();
			}
		}
	}
	if (i) {   /* a partial "inf" or "nan": no conversion */
		shunget(f);
		shlim(f, 0);
		return 0;
	}

	/* hexadecimal */
	if (c == '0') {
		c = shgetc(f);
		if (lower(c) == 'x') {
			/* keep 17 significant hex digits (at least 65 bits) */
			uint32_t ad[8], bd[8];
			struct big A = { ad, 0 }, B = { bd, 1 };
			int hd = 0, sig = 0;
			c = shgetc(f);
			for (;; c = shgetc(f)) {
				int v;
				if (c == rad && !gotrad) { gotrad = 1; continue; }
				if ((unsigned)c - '0' < 10) v = c - '0';
				else if ((unsigned)lower(c) - 'a' < 6) v = lower(c) - 'a' + 10;
				else break;
				gotdig = 1;
				if (!sig && !v) { if (gotrad) ex -= 4; continue; }
				sig = 1;
				if (hd < 17) { big_mul_add(&A, 16, (uint32_t)v); hd++; if (gotrad) ex -= 4; }
				else { if (v) lost = 1; if (!gotrad) ex += 4; }
			}
			if (!gotdig) {
				/* "0x" without digits: just the 0 */
				shunget(f);
				if (pok) { shunget(f); if (gotrad) shunget(f); }
				else shlim(f, 0);
				return neg ? -0.0L : 0.0L;
			}
			if (lower(c) == 'p') {
				int eneg = 0, any = 0, signed_ = 0;
				long long v = 0;
				c = shgetc(f);
				if (c == '+' || c == '-') { eneg = c == '-'; signed_ = 1; c = shgetc(f); }
				for (; (unsigned)c - '0' < 10; c = shgetc(f)) { any = 1; if (v < 100000000) v = v * 10 + (c - '0'); }
				if (!any) {
					/* not an exponent: give back "p" and the sign */
					shunget(f);
					if (!pok) { shlim(f, 0); return 0; }
					if (signed_) shunget(f);
					shunget(f);
					c = -1;
				} else {
					ex += eneg ? -v : v;
				}
			}
			if (c >= 0) shunget(f);
			if (!A.n) return neg ? -0.0L : 0.0L;
			if (lost) { big_mul_add(&A, 16, 1); ex -= 4; }   /* sticky, far below */
			bd[0] = 1;
			if (ex > 20000) { errno = ERANGE; r = overflow(); }
			else if (ex < -20000) { errno = ERANGE; r = 0; }
			else r = convert(&A, &B, (long)ex, P, emin, emax, 0);
			return neg ? -r : r;
		}
		gotdig = 1;
		/* a leading zero: continue as decimal */
		while (c == '0') c = shgetc(f);
	}

	/* decimal */
	for (;; c = shgetc(f)) {
		if (c == rad && !gotrad) { gotrad = 1; continue; }
		if ((unsigned)c - '0' >= 10) break;
		gotdig = 1;
		if (c == '0' && !nd) { if (gotrad) e10--; continue; }
		if (nd < MAXDIG) { digits[nd++] = (char)c; if (gotrad) e10--; }
		else { if (c != '0') lost = 1; if (!gotrad) e10++; }
	}
	if (!gotdig) {   /* no digits: no conversion */
		shunget(f);
		shlim(f, 0);
		return 0;
	}
	if (lower(c) == 'e') {
		int eneg = 0, any = 0, signed_ = 0;
		long long v = 0;
		c = shgetc(f);
		if (c == '+' || c == '-') { eneg = c == '-'; signed_ = 1; c = shgetc(f); }
		for (; (unsigned)c - '0' < 10; c = shgetc(f)) { any = 1; if (v < 100000000) v = v * 10 + (c - '0'); }
		if (!any) {
			/* not an exponent: give back "e" and the sign */
			shunget(f);
			if (pok) { if (signed_) shunget(f); shunget(f); }
			else { shlim(f, 0); return 0; }
			c = -1;
		} else {
			e10 += eneg ? -v : v;
		}
	}
	if (c >= 0) shunget(f);
	if (!nd) return neg ? -0.0L : 0.0L;
	if (lost) { digits[nd++] = '1'; e10--; }   /* sticky: just above the truncation */
	digits[nd] = 0;

	/* out of range in any precision */
	if (nd + e10 > 4935) { errno = ERANGE; return neg ? -overflow() : overflow(); }
	if (nd + e10 < -4955) { errno = ERANGE; return neg ? -0.0L : 0.0L; }

	{
		/* A = digits * 10^max(e10,0), B = 10^max(-e10,0) */
		long long ae = e10 > 0 ? e10 : 0, be = e10 < 0 ? -e10 : 0;
		/* both numbers are shifted to the size of the larger one */
		long la = (long)((nd + ae) * 3402 / 1024 / 32), lb = (long)(be * 3402 / 1024 / 32);
		long limbs = (la > lb ? la : lb) + 8;
		uint32_t ad[limbs], bd[limbs];
		struct big A = { ad, 0 }, B = { bd, 1 };
		int j;
		bd[0] = 1;
		for (j = 0; j < nd; ) {
			uint32_t chunk = 0, mul = 1;
			int k2;
			for (k2 = 0; k2 < 9 && j < nd; k2++, j++) { chunk = chunk * 10 + (uint32_t)(digits[j] - '0'); mul *= 10; }
			big_mul_add(&A, mul, chunk);
		}
		for (; ae >= 9; ae -= 9) big_mul_add(&A, 1000000000u, 0);
		for (; ae > 0; ae--) big_mul_add(&A, 10, 0);
		for (; be >= 9; be -= 9) big_mul_add(&B, 1000000000u, 0);
		for (; be > 0; be--) big_mul_add(&B, 10, 0);
		r = convert(&A, &B, 0, P, emin, emax, 0);
	}
	return neg ? -r : r;
}
