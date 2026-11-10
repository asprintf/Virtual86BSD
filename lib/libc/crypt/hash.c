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

/* MD5 (RFC 1321), SHA-256 and SHA-512 (FIPS 180-4), sharing the
 * buffering and padding. */
#include <string.h>
#include "hash.h"
#include "hash_tables.h"

#define ROL(x, n) ((x) << (n) | (x) >> (32 - (n)))
#define ROR32(x, n) ((x) >> (n) | (x) << (32 - (n)))
#define ROR64(x, n) ((x) >> (n) | (x) << (64 - (n)))

static void md5_block(struct __hash *h, const unsigned char *p)
{
	static const unsigned char r[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };
	uint32_t m[16], a = h->s.w[0], b = h->s.w[1], c = h->s.w[2], d = h->s.w[3], f, t;
	int i, g;
	for (i = 0; i < 16; i++)
		m[i] = p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
	for (i = 0; i < 64; i++) {
		switch (i >> 4) {
		case 0: f = (b & c) | (~b & d); g = i; break;
		case 1: f = (d & b) | (~d & c); g = (5 * i + 1) & 15; break;
		case 2: f = b ^ c ^ d; g = (3 * i + 5) & 15; break;
		default: f = c ^ (b | ~d); g = (7 * i) & 15; break;
		}
		t = d;
		d = c;
		c = b;
		f += a + __md5_k[i] + m[g];
		b += ROL(f, r[(i >> 4) * 4 + (i & 3)]);
		a = t;
	}
	h->s.w[0] += a; h->s.w[1] += b; h->s.w[2] += c; h->s.w[3] += d;
}

static void sha256_block(struct __hash *h, const unsigned char *p)
{
	uint32_t w[64], v[8], t1, t2;
	int i;
	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (; i < 64; i++) {
		uint32_t s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ w[i - 15] >> 3;
		uint32_t s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ w[i - 2] >> 10;
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	memcpy(v, h->s.w, sizeof v);
	for (i = 0; i < 64; i++) {
		t1 = v[7] + (ROR32(v[4], 6) ^ ROR32(v[4], 11) ^ ROR32(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + __sha256_k[i] + w[i];
		t2 = (ROR32(v[0], 2) ^ ROR32(v[0], 13) ^ ROR32(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
		memmove(v + 1, v, 7 * sizeof *v);
		v[4] += t1;
		v[0] = t1 + t2;
	}
	for (i = 0; i < 8; i++)
		h->s.w[i] += v[i];
}

static void sha512_block(struct __hash *h, const unsigned char *p)
{
	uint64_t w[80], v[8], t1, t2;
	int i, j;
	for (i = 0; i < 16; i++)
		for (w[i] = 0, j = 0; j < 8; j++)
			w[i] = w[i] << 8 | p[8 * i + j];
	for (; i < 80; i++) {
		uint64_t s0 = ROR64(w[i - 15], 1) ^ ROR64(w[i - 15], 8) ^ w[i - 15] >> 7;
		uint64_t s1 = ROR64(w[i - 2], 19) ^ ROR64(w[i - 2], 61) ^ w[i - 2] >> 6;
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	memcpy(v, h->s.d, sizeof v);
	for (i = 0; i < 80; i++) {
		t1 = v[7] + (ROR64(v[4], 14) ^ ROR64(v[4], 18) ^ ROR64(v[4], 41)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + __sha512_k[i] + w[i];
		t2 = (ROR64(v[0], 28) ^ ROR64(v[0], 34) ^ ROR64(v[0], 39)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
		memmove(v + 1, v, 7 * sizeof *v);
		v[4] += t1;
		v[0] = t1 + t2;
	}
	for (i = 0; i < 8; i++)
		h->s.d[i] += v[i];
}

void __hash_init(struct __hash *h, int alg)
{
	h->len = 0;
	switch (alg) {
	case __MD5:
		h->bs = 64; h->outlen = 16; h->block = md5_block;
		h->s.w[0] = 0x67452301; h->s.w[1] = 0xefcdab89; h->s.w[2] = 0x98badcfe; h->s.w[3] = 0x10325476;
		break;
	case __SHA256:
		h->bs = 64; h->outlen = 32; h->block = sha256_block;
		memcpy(h->s.w, __sha256_h, sizeof h->s.w);
		break;
	default:
		h->bs = 128; h->outlen = 64; h->block = sha512_block;
		memcpy(h->s.d, __sha512_h, sizeof h->s.d);
		break;
	}
}

void __hash_update(struct __hash *h, const void *data, size_t n)
{
	const unsigned char *p = data;
	unsigned have = (unsigned)(h->len % h->bs), k;
	h->len += n;
	if (have) {
		k = h->bs - have;
		if (n < k) {
			memcpy(h->buf + have, p, n);
			return;
		}
		memcpy(h->buf + have, p, k);
		h->block(h, h->buf);
		p += k;
		n -= k;
	}
	for (; n >= h->bs; p += h->bs, n -= h->bs)
		h->block(h, p);
	memcpy(h->buf, p, n);
}

void __hash_final(struct __hash *h, unsigned char *out)
{
	uint64_t bits = h->len * 8;
	unsigned have = (unsigned)(h->len % h->bs), lenpos = h->bs - (h->bs == 128 ? 16 : 8), i;
	int md5 = h->block == md5_block;
	h->buf[have++] = 0x80;
	if (have > lenpos) {
		memset(h->buf + have, 0, h->bs - have);
		h->block(h, h->buf);
		have = 0;
	}
	memset(h->buf + have, 0, h->bs - have);
	for (i = 0; i < 8; i++)
		h->buf[md5 ? h->bs - 8 + i : h->bs - 1 - i] = (unsigned char)(bits >> 8 * i);
	h->block(h, h->buf);
	for (i = 0; i < h->outlen; i++) {
		if (md5)
			out[i] = (unsigned char)(h->s.w[i / 4] >> 8 * (i % 4));
		else if (h->bs == 64)
			out[i] = (unsigned char)(h->s.w[i / 4] >> 8 * (3 - i % 4));
		else
			out[i] = (unsigned char)(h->s.d[i / 8] >> 8 * (7 - i % 8));
	}
}
