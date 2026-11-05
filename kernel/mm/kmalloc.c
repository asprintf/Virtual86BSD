/* kmalloc.c - kernel heap: size classes of 16..2048 bytes carved from
   pages, larger requests as whole blocks of pages.  Memory comes from the
   direct map. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"

#define NCLASS 8 /* 16, 32, ..., 2048 */
static void *free_obj[NCLASS];

static int size_class(size_t n) {
  int c = 0;
  size_t s = 16;
  while (s < n) {
    s <<= 1;
    c++;
  }
  return c;
}

void *kmalloc(size_t n) {
  if (n == 0)
    n = 1;
  if (n > 2048) {
    int order = 0;
    while ((PAGE_SIZE << order) < n)
      order++;
    uint64_t p = alloc_pages(order);
    if (!p)
      return NULL;
    phys_page(p)->flags = PG_LARGE;
    phys_page(p)->order = (uint8_t)order;
    return P2V(p);
  }
  int c = size_class(n);
  uint64_t f = irq_save();
  if (!free_obj[c]) {
    uint64_t p = alloc_page();
    if (!p) {
      irq_restore(f);
      return NULL;
    }
    phys_page(p)->flags = PG_SLAB;
    phys_page(p)->order = (uint8_t)c;
    size_t sz = 16UL << c;
    char *base = P2V(p);
    for (size_t off = 0; off + sz <= PAGE_SIZE; off += sz) {
      *(void **)(base + off) = free_obj[c];
      free_obj[c] = base + off;
    }
  }
  void *o = free_obj[c];
  free_obj[c] = *(void **)o;
  irq_restore(f);
  return o;
}

void *kzalloc(size_t n) {
  void *p = kmalloc(n);
  if (p)
    memset(p, 0, n);
  return p;
}

void kfree(void *p) {
  if (!p)
    return;
  uint64_t phys = V2P(p);
  struct page *pg = phys_page(PAGE_DOWN(phys));
  if (pg->flags & PG_LARGE) {
    pg->flags = 0;
    free_pages(PAGE_DOWN(phys), pg->order);
    return;
  }
  assert(pg->flags & PG_SLAB);
  uint64_t f = irq_save();
  int c = pg->order;
  *(void **)p = free_obj[c];
  free_obj[c] = p;
  irq_restore(f);
}

void *krealloc(void *p, size_t old, size_t n) {
  void *q = kmalloc(n);
  if (!q)
    return NULL;
  if (p) {
    memcpy(q, p, MIN(old, n));
    kfree(p);
  }
  return q;
}

char *kstrdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *d = kmalloc(n);
  if (d)
    memcpy(d, s, n);
  return d;
}
