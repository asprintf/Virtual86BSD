#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "out.h"

#define SLOTS 2000
static unsigned char *p[SLOTS];
static size_t sz[SLOTS];

static void fill(int i) { size_t k; for (k = 0; k < sz[i]; k++) p[i][k] = (unsigned char)(i * 31 + k); }
static int check(int i) { size_t k; for (k = 0; k < sz[i]; k++) if (p[i][k] != (unsigned char)(i * 31 + k)) return 0; return 1; }

static size_t rsize(void)
{
	switch (rng() % 10) {
	case 0: return rng() % 600000;      /* may be mmapped */
	case 1: case 2: return rng() % 5000;
	default: return rng() % 300;
	}
}

int main(void)
{
	int it, i;
	for (it = 0; it < 300000; it++) {
		i = (int)(rng() % SLOTS);
		switch (rng() % 4) {
		case 0: case 1:
			if (p[i]) { CHECK(check(i)); free(p[i]); }
			sz[i] = rsize(); p[i] = malloc(sz[i]);
			CHECK(p[i] != 0); CHECK(((uintptr_t)p[i] & 15) == 0);
			fill(i); break;
		case 2:
			if (p[i]) {
				size_t ns = rsize(), keep = ns < sz[i] ? ns : sz[i];
				unsigned char *q; size_t k; int ok = check(i);
				CHECK(ok);
				q = realloc(p[i], ns); CHECK(q != 0);
				for (k = 0; k < keep; k++) if (q[k] != (unsigned char)(i * 31 + k)) { CHECK(!"realloc lost data"); break; }
				p[i] = q; sz[i] = ns; fill(i);
			}
			break;
		case 3:
			if (p[i]) { CHECK(check(i)); free(p[i]); p[i] = 0; sz[i] = 0; }
			break;
		}
	}
	for (i = 0; i < SLOTS; i++) { if (p[i]) CHECK(check(i)); free(p[i]); }

	/* calloc */
	for (it = 0; it < 2000; it++) {
		size_t n = rsize(), k; unsigned char *c = calloc(n, 1);
		CHECK(c != 0);
		for (k = 0; k < n; k++) if (c[k]) { CHECK(!"calloc not zero"); break; }
		memset(c, 0xAA, n); free(c);
	}
	errno = 0; CHECK(calloc(SIZE_MAX / 2, 3) == 0 && errno == ENOMEM);
	errno = 0; CHECK(malloc(SIZE_MAX) == 0 && errno == ENOMEM);
	CHECK(reallocarray(0, SIZE_MAX / 2, 3) == 0);

	/* alignment */
	for (it = 0; it < 5000; it++) {
		size_t al = (size_t)1 << (rng() % 14), n = rsize();
		void *a = 0; int r;
		if (al < sizeof(void *)) al = sizeof(void *);
		r = posix_memalign(&a, al, n);
		CHECK(r == 0); CHECK(((uintptr_t)a & (al - 1)) == 0);
		memset(a, 1, n);
		if (it & 1) free(a); else { a = realloc(a, n + 100); CHECK(a != 0); free(a); }
		a = aligned_alloc(al, n); CHECK(a && ((uintptr_t)a & (al - 1)) == 0); free(a);
	}
	{ void *a; CHECK(posix_memalign(&a, 3, 10) == EINVAL); CHECK(posix_memalign(&a, 4, 10) == EINVAL);
	  /* through a pointer: gcc knows aligned_alloc as a builtin and, at -O2,
	   * does not call the library with an alignment that is not a power of 2 */
	  void *(*volatile aa)(size_t, size_t) = aligned_alloc;
	  errno = 0; CHECK(aa(3, 10) == 0 && errno == EINVAL); }
	free(0);
	CHECK(realloc(0, 10) != 0);
	os("malloc ok\n"); oflush();
	return failures != 0;
}
