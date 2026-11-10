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

/* crypt: the traditional DES scheme (two salt characters), BSDi's
 * extended DES ("_" + 4 count + 4 salt characters), and the MD5 ("$1$"),
 * SHA-256 ("$5$") and SHA-512 ("$6$") schemes of FreeBSD and glibc.
 * Unsupported or malformed settings give a null pointer with EINVAL. */
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "des.h"
#include "hash.h"

static const char b64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char out[128];

static int a64(int c)
{
	const char *p = c ? strchr(b64, c) : 0;
	return p ? (int)(p - b64) : -1;
}

/* n characters of v, least significant six bits first */
static char *to64(char *p, unsigned long v, int n)
{
	while (n--) {
		*p++ = b64[v & 63];
		v >>= 6;
	}
	return p;
}

/* the 64-bit block as 11 characters, most significant six bits first */
static char *block64(char *p, uint64_t b)
{
	int i;
	for (i = 0; i < 11; i++)
		*p++ = b64[i < 10 ? b >> (58 - 6 * i) & 63 : (b << 2) & 63];
	return p;
}

static uint64_t key8(const char **kp)
{
	const unsigned char *k = (const unsigned char *)*kp;
	uint64_t r = 0;
	int i;
	for (i = 0; i < 8; i++) {
		r = r << 8 | (uint64_t)(*k << 1 & 0xff);
		if (*k)
			k++;
	}
	*kp = (const char *)k;
	return r;
}

static char *des_trad(const char *key, const char *salt)
{
	struct __des_key k;
	uint64_t b = 0;
	int s0 = a64(salt[0]), s1 = s0 < 0 ? -1 : a64(salt[1]), i;
	char *p = out;
	if (s1 < 0)
		return 0;
	__des_setkey(&k, key8(&key));
	for (i = 0; i < 25; i++)
		b = __des_crypt(&k, b, (uint32_t)(s0 | s1 << 6), 0);
	*p++ = salt[0];
	*p++ = salt[1];
	*block64(p, b) = 0;
	return out;
}

static char *des_ext(const char *key, const char *setting)
{
	struct __des_key k;
	unsigned long count = 0, salt = 0;
	uint64_t kb, b = 0;
	int i, v;
	for (i = 1; i < 9; i++) {
		if ((v = a64(setting[i])) < 0)
			return 0;
		if (i < 5) count |= (unsigned long)v << 6 * (i - 1);
		else salt |= (unsigned long)v << 6 * (i - 5);
	}
	kb = key8(&key);
	__des_setkey(&k, kb);
	while (*key) {
		/* fold in the next eight characters: encrypt the key with itself */
		const char *q = key;
		uint64_t next;
		kb = __des_crypt(&k, kb, 0, 0);
		next = key8(&q);
		kb ^= next;   /* the characters past the end are 0 */
		key = q;
		__des_setkey(&k, kb);
	}
	if (!count)
		count = 1;          /* as libxcrypt */
	while (count--)
		b = __des_crypt(&k, b, (uint32_t)salt, 0);
	memcpy(out, setting, 9);
	*block64(out + 9, b) = 0;
	return out;
}

static char *md5crypt(const char *key, const char *setting)
{
	struct __hash c, a;
	unsigned char f[16];
	const char *salt = setting + 3, *e;
	size_t kl = strlen(key), sl, i;
	char *p;
	static const unsigned char perm[5][3] = { { 0, 6, 12 }, { 1, 7, 13 }, { 2, 8, 14 }, { 3, 9, 15 }, { 4, 10, 5 } };
	for (e = salt; *e && *e != '$' && e - salt < 8; e++)
		;
	sl = (size_t)(e - salt);
	__hash_init(&c, __MD5);
	__hash_update(&c, key, kl);
	__hash_update(&c, "$1$", 3);
	__hash_update(&c, salt, sl);
	__hash_init(&a, __MD5);
	__hash_update(&a, key, kl);
	__hash_update(&a, salt, sl);
	__hash_update(&a, key, kl);
	__hash_final(&a, f);
	for (i = kl; i > 16; i -= 16)
		__hash_update(&c, f, 16);
	__hash_update(&c, f, i);
	for (i = kl; i; i >>= 1)
		__hash_update(&c, i & 1 ? "" : key, 1);
	__hash_final(&c, f);
	for (i = 0; i < 1000; i++) {
		__hash_init(&c, __MD5);
		if (i & 1) __hash_update(&c, key, kl);
		else __hash_update(&c, f, 16);
		if (i % 3) __hash_update(&c, salt, sl);
		if (i % 7) __hash_update(&c, key, kl);
		if (i & 1) __hash_update(&c, f, 16);
		else __hash_update(&c, key, kl);
		__hash_final(&c, f);
	}
	p = out;
	memcpy(p, "$1$", 3);
	memcpy(p + 3, salt, sl);
	p += 3 + sl;
	*p++ = '$';
	for (i = 0; i < 5; i++)
		p = to64(p, (unsigned long)f[perm[i][0]] << 16 | (unsigned long)f[perm[i][1]] << 8 | f[perm[i][2]], 4);
	p = to64(p, f[11], 2);
	*p = 0;
	return out;
}

/* "rounds=N$": N, or 0 if absent; -1 if malformed */
static long rounds(const char **sp)
{
	const char *s = *sp;
	char *end;
	unsigned long n;
	if (strncmp(s, "rounds=", 7))
		return 0;
	s += 7;
	if (*s < '0' || *s > '9')
		return -1;
	n = strtoul(s, &end, 10);
	if (*end != '$')
		return -1;
	*sp = end + 1;
	if (n < 1000) n = 1000;
	if (n > 999999999) n = 999999999;
	return (long)n;
}

static void repeat(struct __hash *h, const unsigned char *b, size_t blen, size_t n)
{
	for (; n > blen; n -= blen)
		__hash_update(h, b, blen);
	__hash_update(h, b, n);
}

static char *shacrypt(const char *key, const char *setting, int alg)
{
	static const unsigned char perm256[10][3] = {
		{ 0, 10, 20 }, { 21, 1, 11 }, { 12, 22, 2 }, { 3, 13, 23 }, { 24, 4, 14 }, { 15, 25, 5 },
		{ 6, 16, 26 }, { 27, 7, 17 }, { 18, 28, 8 }, { 9, 19, 29 },
	};
	static const unsigned char perm512[21][3] = {
		{ 0, 21, 42 }, { 22, 43, 1 }, { 44, 2, 23 }, { 3, 24, 45 }, { 25, 46, 4 }, { 47, 5, 26 },
		{ 6, 27, 48 }, { 28, 49, 7 }, { 50, 8, 29 }, { 9, 30, 51 }, { 31, 52, 10 }, { 53, 11, 32 },
		{ 12, 33, 54 }, { 34, 55, 13 }, { 56, 14, 35 }, { 15, 36, 57 }, { 37, 58, 16 }, { 59, 17, 38 },
		{ 18, 39, 60 }, { 40, 61, 19 }, { 62, 20, 41 },
	};
	struct __hash c;
	unsigned char a[64], dp[64], ds[64];
	const char *salt = setting + 3, *e;
	size_t kl = strlen(key), sl, i, hl = alg == __SHA256 ? 32 : 64, n;
	long r = rounds(&salt), nr = r ? r : 5000, k;
	char *p;
	if (r < 0)
		return 0;
	for (e = salt; *e && *e != '$' && e - salt < 16; e++)
		;
	sl = (size_t)(e - salt);

	__hash_init(&c, alg);                      /* B */
	__hash_update(&c, key, kl);
	__hash_update(&c, salt, sl);
	__hash_update(&c, key, kl);
	__hash_final(&c, a);
	__hash_init(&c, alg);                      /* A */
	__hash_update(&c, key, kl);
	__hash_update(&c, salt, sl);
	repeat(&c, a, hl, kl);
	for (i = kl; i; i >>= 1)
		if (i & 1) __hash_update(&c, a, hl);
		else __hash_update(&c, key, kl);
	__hash_final(&c, a);
	__hash_init(&c, alg);                      /* DP */
	for (i = 0; i < kl; i++)
		__hash_update(&c, key, kl);
	__hash_final(&c, dp);
	__hash_init(&c, alg);                      /* DS */
	for (i = 0; i < 16u + a[0]; i++)
		__hash_update(&c, salt, sl);
	__hash_final(&c, ds);
	for (k = 0; k < nr; k++) {
		__hash_init(&c, alg);
		if (k & 1) repeat(&c, dp, hl, kl);
		else __hash_update(&c, a, hl);
		if (k % 3) repeat(&c, ds, hl, sl);
		if (k % 7) repeat(&c, dp, hl, kl);
		if (k & 1) __hash_update(&c, a, hl);
		else repeat(&c, dp, hl, kl);
		__hash_final(&c, a);
	}
	p = out;
	memcpy(p, setting, 3);
	p += 3;
	if (r) {
		memcpy(p, "rounds=", 7);
		p += 7;
		{
			char d[12];
			int nd = 0;
			unsigned long v = (unsigned long)r;
			do d[nd++] = (char)('0' + v % 10); while (v /= 10);
			while (nd) *p++ = d[--nd];
		}
		*p++ = '$';
	}
	memcpy(p, salt, sl);
	p += sl;
	*p++ = '$';
	n = alg == __SHA256 ? 10 : 21;
	for (i = 0; i < n; i++) {
		const unsigned char *t = alg == __SHA256 ? perm256[i] : perm512[i];
		p = to64(p, (unsigned long)a[t[0]] << 16 | (unsigned long)a[t[1]] << 8 | a[t[2]], 4);
	}
	if (alg == __SHA256)
		p = to64(p, (unsigned long)a[31] << 8 | a[30], 3);
	else
		p = to64(p, a[63], 2);
	*p = 0;
	return out;
}

char *crypt(const char *key, const char *salt)
{
	char *r;
	if (salt[0] == '$' && salt[1] && salt[2] == '$') {
		switch (salt[1]) {
		case '1': r = md5crypt(key, salt); break;
		case '5': r = shacrypt(key, salt, __SHA256); break;
		case '6': r = shacrypt(key, salt, __SHA512); break;
		default: r = 0;
		}
	} else if (salt[0] == '_')
		r = des_ext(key, salt);
	else
		r = des_trad(key, salt);
	if (!r)
		errno = EINVAL;
	return r;
}
