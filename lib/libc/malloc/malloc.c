/* malloc family.
 *
 * Design (deliberately simple, to be tuned later):
 *  - Small and medium requests come from "regions" obtained with mmap and
 *    are managed as boundary-tagged chunks with immediate coalescing.
 *    Free chunks live in 64 segregated, size-ordered-by-bin lists; a
 *    bitmap finds the first non-empty bin quickly.
 *  - Requests of MMAP_THRESHOLD bytes or more get their own mapping,
 *    returned to the system on free().
 *  - One global lock protects the heap.
 *
 * Chunk layout (all sizes multiples of 16, payload 16-byte aligned):
 *
 *   +0  psize   size of the previous chunk, valid only if it is free
 *   +8  size    this chunk's size | C_INUSE | P_INUSE | C_MMAP
 *   +16 payload (free chunks: next, prev list links)
 *
 * Each region ends with a zero-sized, in-use fence chunk header.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "lock.h"
#include "syscall.h"

#define ALIGN          16
#define HDR            16
#define MINCHUNK       32
#define C_INUSE        1UL
#define P_INUSE        2UL
#define C_MMAP         4UL
#define FLAGS          7UL
#define REGION_MIN     (1UL << 20)
#define MMAP_THRESHOLD (256UL << 10)
#define NBINS          64

struct chunk {
	size_t psize, size;
	struct chunk *next, *prev; /* free chunks only */
};

#define CSIZE(c)     ((c)->size & ~FLAGS)
#define MEM(c)       ((void *)((char *)(c) + HDR))
#define CHUNK(p)     ((struct chunk *)((char *)(p) - HDR))
#define NEXT(c)      ((struct chunk *)((char *)(c) + CSIZE(c)))
#define PREV(c)      ((struct chunk *)((char *)(c) - (c)->psize))

static volatile int heap_lock;
static struct chunk bins[NBINS];   /* list heads (only next/prev used) */
static uint64_t binmap;
static int bins_ready;

static void *sys_mmap(size_t n)
{
	long r = __syscall6x(SYS_mmap, 0, n, 3 /*RW*/, 0x22 /*PRIVATE|ANON*/, -1, 0);
	return (unsigned long)r > -4096UL ? 0 : (void *)r;
}

static size_t page_round(size_t n) { return (n + 4095) & ~(size_t)4095; }

/* Request size -> chunk size, or 0 on overflow. */
static size_t req2size(size_t n)
{
	if (n > SIZE_MAX / 2) return 0;
	n = (n + HDR + ALIGN - 1) & ~(size_t)(ALIGN - 1);
	return n < MINCHUNK ? MINCHUNK : n;
}

static int bin_index(size_t size)
{
	size_t u = size >> 4;   /* 16-byte units, >= 2 */
	int lg;
	if (u < 34) return (int)u - 2;               /* bins 0..31: exact, 32..528 bytes */
	lg = 0;
	while ((u >> lg) > 1) lg++;                  /* floor(log2 u), >= 5 */
	{
		int b = 32 + 4 * (lg - 5) + (int)((u >> (lg - 2)) & 3);
		return b < NBINS ? b : NBINS - 1;
	}
}

static void init_bins(void)
{
	int i;
	for (i = 0; i < NBINS; i++) bins[i].next = bins[i].prev = &bins[i];
	bins_ready = 1;
}

static void unbin(struct chunk *c)
{
	c->prev->next = c->next;
	c->next->prev = c->prev;
	if (c->next == c->prev) { /* list may now be empty */
		int i = bin_index(CSIZE(c));
		if (bins[i].next == &bins[i]) binmap &= ~(1ULL << i);
	}
}

static void bin(struct chunk *c)
{
	int i = bin_index(CSIZE(c));
	struct chunk *h = &bins[i];
	c->next = h->next;
	c->prev = h;
	h->next->prev = c;
	h->next = c;
	binmap |= 1ULL << i;
}

/* Mark c (size sz, free) and fix the boundary tags around it. */
static void set_free(struct chunk *c, size_t sz)
{
	struct chunk *n;
	c->size = sz | (c->size & P_INUSE);
	n = NEXT(c);
	n->psize = sz;
	n->size &= ~P_INUSE;
}

/* Split chunk c (in use) so that it is exactly sz bytes, freeing the tail. */
static void trim(struct chunk *c, size_t sz)
{
	size_t total = CSIZE(c);
	struct chunk *rest, *n;
	if (total - sz < MINCHUNK) return;
	c->size = sz | (c->size & (P_INUSE | C_INUSE));
	rest = NEXT(c);
	rest->size = (total - sz) | P_INUSE;
	n = NEXT(rest);
	if (!(n->size & C_INUSE)) { /* coalesce forward */
		unbin(n);
		rest->size += CSIZE(n);
	}
	set_free(rest, CSIZE(rest));
	bin(rest);
}

static struct chunk *new_region(size_t need)
{
	size_t len = page_round(need + 2 * HDR);
	char *mem;
	struct chunk *c, *fence;
	if (len < REGION_MIN) len = REGION_MIN;
	mem = sys_mmap(len);
	if (!mem) return 0;
	/* mem is page aligned; the first chunk starts at mem so that its
	 * payload is 16-aligned; the fence occupies the last 16 bytes. */
	c = (struct chunk *)mem;
	c->psize = 0;
	c->size = (len - HDR) | P_INUSE;
	fence = NEXT(c);
	fence->size = 0 | C_INUSE;
	fence->psize = 0;
	return c;
}

static struct chunk *find(size_t sz)
{
	int i = bin_index(sz);
	uint64_t m;
	struct chunk *c;
	/* Search the request's own bin (sizes may be mixed in log bins). */
	for (c = bins[i].next; c != &bins[i]; c = c->next)
		if (CSIZE(c) >= sz) { unbin(c); return c; }
	m = i + 1 < NBINS ? binmap & (~0ULL << (i + 1)) : 0;
	if (m) {
		for (i = 0; !(m & 1); m >>= 1, i++);   /* lowest set bit = first non-empty bin */
		c = bins[i].next;
		unbin(c);
		return c;
	}
	return 0;
}

static void *alloc_mmapped(size_t sz, size_t align)
{
	/* psize holds the offset of the chunk from the start of the mapping. */
	size_t len = page_round(sz + align + HDR);
	char *mem = sys_mmap(len);
	uintptr_t p;
	struct chunk *c;
	if (!mem) return 0;
	p = ((uintptr_t)mem + HDR + align - 1) & -(uintptr_t)align;
	c = CHUNK(p);
	c->psize = (size_t)((char *)c - mem);
	c->size = (len - c->psize) | C_MMAP | C_INUSE;
	return (void *)p;
}

static void *alloc_locked(size_t sz)
{
	struct chunk *c = find(sz), *n;
	if (!c) {
		c = new_region(sz);
		if (!c) return 0;
	}
	c->size |= C_INUSE;
	n = NEXT(c);
	n->size |= P_INUSE;
	trim(c, sz);
	return MEM(c);
}

void *malloc(size_t n)
{
	size_t sz = req2size(n);
	void *p;
	if (!sz) { errno = ENOMEM; return 0; }
	if (sz >= MMAP_THRESHOLD) {
		p = alloc_mmapped(sz, ALIGN);
		if (!p) errno = ENOMEM;
		return p;
	}
	LOCK(&heap_lock);
	if (!bins_ready) init_bins();
	p = alloc_locked(sz);
	UNLOCK(&heap_lock);
	if (!p) errno = ENOMEM;
	return p;
}

static void free_locked(struct chunk *c)
{
	size_t sz = CSIZE(c);
	struct chunk *n = NEXT(c);
	if (!(n->size & C_INUSE)) { unbin(n); sz += CSIZE(n); }
	if (!(c->size & P_INUSE)) {
		struct chunk *p = PREV(c);
		unbin(p);
		sz += CSIZE(p);
		c = p;
	}
	c->size &= ~C_INUSE;
	set_free(c, sz);
	bin(c);
}

void free(void *p)
{
	struct chunk *c;
	if (!p) return;
	c = CHUNK(p);
	if (c->size & C_MMAP) {
		__syscall2(SYS_munmap, (char *)c - c->psize, CSIZE(c) + c->psize);
		return;
	}
	LOCK(&heap_lock);
	free_locked(c);
	UNLOCK(&heap_lock);
}

void *calloc(size_t m, size_t n)
{
	void *p;
	if (n && m > SIZE_MAX / n) { errno = ENOMEM; return 0; }
	p = malloc(m * n);
	if (p && !(CHUNK(p)->size & C_MMAP)) memset(p, 0, m * n); /* mmap: already zero */
	return p;
}

void *realloc(void *p, size_t n)
{
	struct chunk *c, *nx;
	size_t sz, old;
	void *q;
	if (!p) return malloc(n);
	sz = req2size(n);
	if (!sz) { errno = ENOMEM; return 0; }
	c = CHUNK(p);
	old = CSIZE(c);
	if (c->size & C_MMAP) {
		size_t off = c->psize, len = page_round(sz + off);
		long r;
		if (sz >= MMAP_THRESHOLD / 2) {
			r = __syscall5(SYS_mremap, (char *)c - off, old + off, len, 1 /*MAYMOVE*/, 0);
			if ((unsigned long)r <= -4096UL) {
				c = (struct chunk *)((char *)r + off);
				c->size = (len - off) | C_MMAP | C_INUSE;
				return MEM(c);
			}
		}
		if (sz <= old && sz >= MMAP_THRESHOLD / 2) return p;
	} else {
		LOCK(&heap_lock);
		if (sz <= old) { trim(c, sz); UNLOCK(&heap_lock); return p; }
		nx = NEXT(c);
		if (!(nx->size & C_INUSE) && old + CSIZE(nx) >= sz) {
			unbin(nx);
			c->size += CSIZE(nx);
			NEXT(c)->size |= P_INUSE;
			trim(c, sz);
			UNLOCK(&heap_lock);
			return p;
		}
		UNLOCK(&heap_lock);
	}
	q = malloc(n);
	if (!q) return 0;
	memcpy(q, p, (old - HDR < n ? old - HDR : n));
	free(p);
	return q;
}

void *__alloc_aligned(size_t align, size_t n)
{
	size_t sz = req2size(n);
	struct chunk *c, *a;
	char *p;
	uintptr_t ap;
	if (!sz || align > SIZE_MAX / 4 || sz > SIZE_MAX - align - MINCHUNK) return 0;
	if (align <= ALIGN) return malloc(n);
	if (sz >= MMAP_THRESHOLD) return alloc_mmapped(sz, align);
	LOCK(&heap_lock);
	if (!bins_ready) init_bins();
	p = alloc_locked(sz + align + MINCHUNK);
	if (!p) { UNLOCK(&heap_lock); return 0; }
	c = CHUNK(p);
	ap = ((uintptr_t)p + align - 1) & -(uintptr_t)align;
	if (ap != (uintptr_t)p) {
		/* Ensure the leading gap can be a free chunk of its own. */
		if (ap - (uintptr_t)p < MINCHUNK) ap += align;
		a = CHUNK(ap);
		a->size = (CSIZE(c) - (size_t)((char *)a - (char *)c)) | C_INUSE;
		c->size = (size_t)((char *)a - (char *)c) | (c->size & P_INUSE) | C_INUSE;
		/* now free the leading chunk c; a's P_INUSE is clear after this */
		free_locked(c);
		c = a;
	}
	trim(c, sz);
	UNLOCK(&heap_lock);
	return MEM(c);
}

void *aligned_alloc(size_t align, size_t n)
{
	void *p;
	if (!align || (align & (align - 1))) { errno = EINVAL; return 0; }
	p = __alloc_aligned(align, n);
	if (!p) errno = ENOMEM;
	return p;
}

/* fork(): -1 before (take the lock), 0 in the parent, 1 in the child. */
void __malloc_atfork(int when)
{
	if (when < 0) LOCK(&heap_lock);
	else if (when == 0) UNLOCK(&heap_lock);
	else heap_lock = 0;
}
