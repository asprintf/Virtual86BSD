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

/* The 48-bit linear congruential generators (XSI). */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static unsigned short seed48v[7] = { 0x330e, 0xabcd, 0x1234, 0xe66d, 0xdeec, 0x5, 0xb };
/* seed48v[0..2]: X; [3..5]: a; [6]: c */

static uint64_t next(unsigned short x[3], const unsigned short *p)
{
	uint64_t a = (uint64_t)p[3] | (uint64_t)p[4] << 16 | (uint64_t)p[5] << 32;
	uint64_t v = (uint64_t)x[0] | (uint64_t)x[1] << 16 | (uint64_t)x[2] << 32;
	v = (a * v + p[6]) & 0xffffffffffffULL;
	x[0] = (unsigned short)v; x[1] = (unsigned short)(v >> 16); x[2] = (unsigned short)(v >> 32);
	return v;
}

double erand48(unsigned short x[3])
{
	uint64_t v = next(x, seed48v);
	return (double)v / 281474976710656.0;   /* 2^48 */
}
double drand48(void) { return erand48(seed48v); }
long nrand48(unsigned short x[3]) { return (long)(next(x, seed48v) >> 17); }
long lrand48(void) { return nrand48(seed48v); }
long jrand48(unsigned short x[3]) { return (long)(int32_t)(uint32_t)(next(x, seed48v) >> 16); }
long mrand48(void) { return jrand48(seed48v); }

void srand48(long s)
{
	unsigned short p[7] = { 0x330e, (unsigned short)s, (unsigned short)(s >> 16), 0xe66d, 0xdeec, 0x5, 0xb };
	memcpy(seed48v, p, sizeof p);
}

unsigned short *seed48(unsigned short s[3])
{
	static unsigned short old[3];
	memcpy(old, seed48v, sizeof old);
	memcpy(seed48v, s, sizeof old);
	seed48v[3] = 0xe66d; seed48v[4] = 0xdeec; seed48v[5] = 0x5; seed48v[6] = 0xb;
	return old;
}

void lcong48(unsigned short p[7]) { memcpy(seed48v, p, sizeof seed48v); }
