#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "clofork.h"
#include "lock.h"
#include "syscall.h"

#define WBITS (8 * sizeof(unsigned long))

volatile int __clofork_any;
static volatile int lock;
static unsigned long builtin[1024 / WBITS];
static unsigned long *map = builtin;
static size_t nwords = sizeof builtin / sizeof *builtin;

int __clofork_set(int fd, int on)
{
	size_t w = (size_t)fd / WBITS;
	if (fd < 0) return 0;
	LOCK(&lock);
	if (w >= nwords) {
		size_t n;
		unsigned long *m;
		if (!on) { UNLOCK(&lock); return 0; }
		n = 2 * w + 2;
		m = malloc(n * sizeof *m);
		if (!m) { UNLOCK(&lock); errno = ENOMEM; return -1; }
		memcpy(m, map, nwords * sizeof *m);
		memset(m + nwords, 0, (n - nwords) * sizeof *m);
		if (map != builtin) free(map);
		map = m;
		nwords = n;
	}
	if (on) { map[w] |= 1UL << (fd % WBITS); __clofork_any = 1; }
	else map[w] &= ~(1UL << (fd % WBITS));
	UNLOCK(&lock);
	return 0;
}

int __clofork_get(int fd)
{
	size_t w = (size_t)fd / WBITS;
	int r;
	if (fd < 0 || !__clofork_any) return 0;
	LOCK(&lock);
	r = w < nwords && (map[w] >> (fd % WBITS) & 1);
	UNLOCK(&lock);
	return r;
}

void __clofork_close_all(void)
{
	size_t w, b;
	if (!__clofork_any) return;
	lock = 0; /* the child has one thread; the parent's holder is gone */
	for (w = 0; w < nwords; w++)
		for (b = 0; map[w] && b < WBITS; b++)
			if (map[w] >> b & 1) {
				__syscall1(SYS_close, (long)(w * WBITS + b));
				map[w] &= ~(1UL << b);
			}
}
