/* DES (FIPS 46-3).  The standard tables (bit 1 is the most significant)
 * give the key schedule and, once, the combined tables of the rounds. */
#include "des.h"
#include "atomic.h"
#include "lock.h"

static const unsigned char ip[64] = {
	58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
	62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
	57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
	61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7,
};
static const unsigned char pbox[32] = {
	16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
	2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25,
};
static const unsigned char pc1[56] = {
	57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
	10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
	14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4,
};
static const unsigned char pc2[48] = {
	14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10,
	23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,
};
static const unsigned char shifts[16] = { 1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 };
static const unsigned char sbox[8][64] = {
	{ 14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
	  0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
	  4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
	  15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13 },
	{ 15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
	  3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
	  0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
	  13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9 },
	{ 10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
	  13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
	  13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
	  1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12 },
	{ 7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
	  13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
	  10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
	  3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14 },
	{ 2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
	  14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
	  4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
	  11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3 },
	{ 12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
	  10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
	  9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
	  4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13 },
	{ 4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
	  13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
	  1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
	  6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12 },
	{ 13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
	  1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
	  7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
	  2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11 },
};

/* the n bits of `in' (inbits wide) selected by t, MSB first */
static uint64_t perm(uint64_t in, const unsigned char *t, int n, int inbits)
{
	uint64_t o = 0;
	int i;
	for (i = 0; i < n; i++)
		o = o << 1 | (in >> (inbits - t[i]) & 1);
	return o;
}

void __des_setkey(struct __des_key *k, uint64_t key)
{
	uint64_t cd = perm(key, pc1, 56, 64);
	uint32_t c = (uint32_t)(cd >> 28), d = (uint32_t)(cd & 0xfffffff);
	int i;
	for (i = 0; i < 16; i++) {
		c = (c << shifts[i] | c >> (28 - shifts[i])) & 0xfffffff;
		d = (d << shifts[i] | d >> (28 - shifts[i])) & 0xfffffff;
		k->k[i] = perm((uint64_t)c << 28 | d, pc2, 48, 56);
	}
}

/* fast tables, made once from the ones above: sp[i][v] is P applied to
 * S-box i's output for input v (in its place); ipt/fpt[j][b] the
 * contribution of byte j (from the most significant) with value b to
 * the initial/final permutation */
static uint32_t sp[8][64];
static uint64_t ipt[8][256], fpt[8][256];
static volatile int ready, lock;

static void mktables(void)
{
	unsigned char fp[64];
	int i, j, b, v;
	LOCK(&lock);
	if (!ready) {
		for (i = 0; i < 64; i++)
			fp[ip[i] - 1] = (unsigned char)(i + 1);
		for (i = 0; i < 8; i++)
			for (v = 0; v < 64; v++) {
				uint32_t s = sbox[i][(v & 32) | (v & 1) << 4 | (v >> 1 & 15)];
				sp[i][v] = (uint32_t)perm((uint64_t)s << (28 - 4 * i), pbox, 32, 32);
			}
		for (j = 0; j < 8; j++)
			for (b = 0; b < 256; b++) {
				uint64_t x = (uint64_t)b << (56 - 8 * j);
				ipt[j][b] = perm(x, ip, 64, 64);
				fpt[j][b] = perm(x, fp, 64, 64);
			}
		a_store(&ready, 1);
	}
	UNLOCK(&lock);
}

static uint64_t bytes(uint64_t x, uint64_t t[8][256])
{
	uint64_t r = 0;
	int j;
	for (j = 0; j < 8; j++)
		r |= t[j][x >> (56 - 8 * j) & 255];
	return r;
}

uint64_t __des_crypt(const struct __des_key *k, uint64_t block, uint32_t salt, int dec)
{
	uint64_t x, e;
	uint32_t l, r, t, s = 0, lo, hi, d, rr;
	int i, j;
	if (!ready)
		mktables();
	/* salt bit i (from the least significant) swaps E bits i and i+24
	 * (from the first): bit i is at 1 << (23 - i) in a 24-bit half */
	for (i = 0; i < 24; i++)
		if (salt >> i & 1)
			s |= 1u << (23 - i);
	x = bytes(block, ipt);
	l = (uint32_t)(x >> 32);
	r = (uint32_t)x;
	for (i = 0; i < 16; i++) {
		/* E: group j is bits 4j..4j+5 (bit 0 = bit 32) of R */
		rr = r >> 1 | r << 31;
		x = (uint64_t)rr << 32 | rr;
		for (e = 0, j = 0; j < 8; j++)
			e = e << 6 | (x >> (58 - 4 * j) & 63);
		hi = (uint32_t)(e >> 24);
		lo = (uint32_t)(e & 0xffffff);
		d = (hi ^ lo) & s;
		e = ((uint64_t)(hi ^ d) << 24 | (lo ^ d)) ^ k->k[dec ? 15 - i : i];
		for (t = 0, j = 0; j < 8; j++)
			t |= sp[j][e >> (42 - 6 * j) & 63];
		t ^= l;
		l = r;
		r = t;
	}
	return bytes((uint64_t)r << 32 | l, fpt);
}
