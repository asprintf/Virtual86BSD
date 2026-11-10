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

/* random(), srandom(), initstate(), setstate(): the additive feedback
 * generator of BSD and glibc (same seeding, same sequences).  State
 * sizes 8, 32, 64, 128 and 256 bytes select degree 0 (an LCG), 7, 15, 31
 * and 63.  Without srandom() the generator behaves as if seeded with 1. */
#include <stdlib.h>
#include <stdint.h>
#include "lock.h"

static uint32_t deftable[32];
static uint32_t *x = deftable + 1;   /* x[-1] records n, i, j */
static int n = 31, i, j, seeded;
static volatile int lock;

static int sep(int deg) { return deg == 7 || deg == 31 ? 3 : 1; }

static uint32_t step(void)
{
	uint32_t k;
	if (n == 0) return x[0] = (1103515245u * x[0] + 12345u) & 0x7fffffff;
	x[i] += x[j];
	k = x[i] >> 1;
	if (++i == n) i = 0;
	if (++j == n) j = 0;
	return k;
}

static void seed(unsigned s)
{
	int k;
	seeded = 1;
	if (n == 0) { x[0] = s; return; }
	x[0] = s ? s : 1;
	for (k = 1; k < n; k++) {
		int32_t w = (int32_t)x[k - 1], hi = w / 127773, lo = w % 127773;
		w = 16807 * lo - 2836 * hi;
		if (w < 0) w += 2147483647;
		x[k] = (uint32_t)w;
	}
	i = sep(n);
	j = 0;
	for (k = 0; k < 10 * n; k++) step();
}

long random(void)
{
	long r;
	LOCK(&lock);
	if (!seeded) seed(1);
	r = (long)step();
	UNLOCK(&lock);
	return r;
}

void srandom(unsigned s)
{
	LOCK(&lock);
	seed(s);
	UNLOCK(&lock);
}

static void save(void) { x[-1] = (uint32_t)n << 16 | (uint32_t)i << 8 | (uint32_t)j; }

char *initstate(unsigned s, char *state, size_t size)
{
	char *old;
	if (size < 8) return 0;
	LOCK(&lock);
	if (!seeded) seed(1);
	old = (char *)(x - 1);
	save();
	n = size < 32 ? 0 : size < 64 ? 7 : size < 128 ? 15 : size < 256 ? 31 : 63;
	x = (uint32_t *)(void *)state + 1;
	seed(s);
	save();
	UNLOCK(&lock);
	return old;
}

char *setstate(char *state)
{
	char *old;
	LOCK(&lock);
	if (!seeded) seed(1);
	old = (char *)(x - 1);
	save();
	x = (uint32_t *)(void *)state + 1;
	n = (int)(x[-1] >> 16);
	i = (int)(x[-1] >> 8 & 0xff);
	j = (int)(x[-1] & 0xff);
	seeded = 1;
	UNLOCK(&lock);
	return old;
}
