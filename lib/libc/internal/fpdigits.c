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

/* Exact decimal expansion of a long double (x86 80-bit extended).
 *
 * A finite x = m * 2^e (m a 64-bit integer) is expanded exactly:
 *   e >= 0:  m * 2^e is an integer;
 *   e <  0:  m * 2^e = m * 5^-e / 10^-e.
 * The big number is kept in base 10^9 limbs.  The result is the digit
 * string without leading or trailing zeros and the decimal exponent
 * such that |x| = 0.d1 d2 d3 ... * 10^dexp.
 */
#include <stdint.h>
#include <string.h>
#include "fpdigits.h"

#define BASE 1000000000u
#define MAXLIMBS 1300

void __ld_parts(long double x, uint64_t *m, int *e, int *sign, int *cls)
{
	union { long double f; struct { uint64_t m; uint16_t se; } i; } u;
	int ex;
	u.f = x;
	*sign = u.i.se >> 15;
	ex = u.i.se & 0x7fff;
	*m = u.i.m;
	if (ex == 0x7fff) {
		*cls = (*m << 1) ? FP_CLS_NAN : FP_CLS_INF;
		*e = 0;
		return;
	}
	if (!*m) { *cls = FP_CLS_ZERO; *e = 0; return; }
	*cls = FP_CLS_FINITE;
	*e = (ex ? ex : 1) - 16383 - 63;
}

/* multiply big (n limbs) by k (< 2^32 / something), return new n */
static int mul(uint32_t *b, int n, uint32_t k)
{
	uint64_t carry = 0;
	int i;
	for (i = 0; i < n; i++) {
		uint64_t t = (uint64_t)b[i] * k + carry;
		b[i] = (uint32_t)(t % BASE);
		carry = t / BASE;
	}
	while (carry) { b[n++] = (uint32_t)(carry % BASE); carry /= BASE; }
	return n;
}

int __fp_digits(uint64_t m, int e, char *digits, int *dexp)
{
	uint32_t big[MAXLIMBS];
	int n = 0, len, i, k;
	char *p;

	if (!m) { digits[0] = 0; *dexp = 0; return 0; }
	/* strip trailing binary zeros: smaller numbers, same value */
	while (!(m & 1)) { m >>= 1; e++; }
	while (m) { big[n++] = (uint32_t)(m % BASE); m /= BASE; }
	if (e > 0) {
		for (; e >= 29; e -= 29) n = mul(big, n, 1u << 29);
		if (e) n = mul(big, n, 1u << e);
		e = 0;
	} else {
		int f = -e;
		for (; f >= 13; f -= 13) n = mul(big, n, 1220703125u);   /* 5^13 */
		for (; f > 0; f--) n = mul(big, n, 5);
	}
	/* to a digit string, most significant first */
	p = digits;
	{
		uint32_t top = big[n - 1];
		char t[10];
		int c = 0;
		do t[c++] = (char)('0' + top % 10); while (top /= 10);
		while (c) *p++ = t[--c];
	}
	for (i = n - 2; i >= 0; i--) {
		uint32_t v = big[i];
		for (k = 8; k >= 0; k--) { p[k] = (char)('0' + v % 10); v /= 10; }
		p += 9;
	}
	len = (int)(p - digits);
	*dexp = len + e;   /* e <= 0 here: value = digits * 10^e */
	while (len > 0 && digits[len - 1] == '0') len--;
	digits[len] = 0;
	return len;
}
