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

/* qsort/qsort_r: introsort (median-of-three quicksort, insertion sort
 * for small ranges, heapsort fallback), O(n log n) worst case.
 * Not stable, as permitted. */
#include <stdlib.h>
#include <string.h>

typedef int (*cmp_r)(const void *, const void *, void *);

static void swap(char *a, char *b, size_t w)
{
	char t[64];
	while (w) {
		size_t k = w < sizeof t ? w : sizeof t;
		memcpy(t, a, k); memcpy(a, b, k); memcpy(b, t, k);
		a += k; b += k; w -= k;
	}
}

static void sift(char *base, size_t root, size_t n, size_t w, cmp_r cmp, void *arg)
{
	for (;;) {
		size_t c = 2 * root + 1;
		if (c >= n) return;
		if (c + 1 < n && cmp(base + c * w, base + (c + 1) * w, arg) < 0) c++;
		if (cmp(base + root * w, base + c * w, arg) >= 0) return;
		swap(base + root * w, base + c * w, w);
		root = c;
	}
}

static void heapsort_(char *base, size_t n, size_t w, cmp_r cmp, void *arg)
{
	size_t i;
	for (i = n / 2; i-- > 0;) sift(base, i, n, w, cmp, arg);
	for (i = n; i-- > 1;) {
		swap(base, base + i * w, w);
		sift(base, 0, i, w, cmp, arg);
	}
}

static void insertion(char *base, size_t n, size_t w, cmp_r cmp, void *arg)
{
	size_t i, j;
	for (i = 1; i < n; i++)
		for (j = i; j > 0 && cmp(base + (j - 1) * w, base + j * w, arg) > 0; j--)
			swap(base + (j - 1) * w, base + j * w, w);
}

static void intro(char *base, size_t n, size_t w, cmp_r cmp, void *arg, int depth)
{
	while (n > 16) {
		char *lo = base, *mid = base + (n / 2) * w, *hi = base + (n - 1) * w;
		size_t i, j;
		if (depth-- == 0) { heapsort_(base, n, w, cmp, arg); return; }
		/* median of three, moved to lo */
		if (cmp(mid, lo, arg) < 0) swap(mid, lo, w);
		if (cmp(hi, mid, arg) < 0) {
			swap(hi, mid, w);
			if (cmp(mid, lo, arg) < 0) swap(mid, lo, w);
		}
		swap(lo, mid, w);
		/* Hoare partition around the pivot at lo */
		i = 0; j = n;
		for (;;) {
			do i++; while (i < n && cmp(base + i * w, lo, arg) < 0);
			do j--; while (cmp(base + j * w, lo, arg) > 0);
			if (i >= j) break;
			swap(base + i * w, base + j * w, w);
		}
		swap(lo, base + j * w, w);
		/* recurse into the smaller part, loop on the larger */
		if (j < n - j - 1) {
			intro(base, j, w, cmp, arg, depth);
			base += (j + 1) * w; n -= j + 1;
		} else {
			intro(base + (j + 1) * w, n - j - 1, w, cmp, arg, depth);
			n = j;
		}
	}
	insertion(base, n, w, cmp, arg);
}

void __qsort_r(void *base, size_t n, size_t w, cmp_r cmp, void *arg)
{
	int depth = 0;
	size_t m;
	if (n < 2 || !w) return;
	for (m = n; m; m >>= 1) depth += 2;
	intro(base, n, w, cmp, arg, depth);
}

static int wrap(const void *a, const void *b, void *f)
{
	return (*(int (**)(const void *, const void *))f)(a, b);
}

void qsort(void *base, size_t n, size_t w, int (*cmp)(const void *, const void *))
{
	__qsort_r(base, n, w, wrap, &cmp);
}
