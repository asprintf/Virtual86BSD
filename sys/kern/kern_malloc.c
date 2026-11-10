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

/* kmalloc.c - kernel heap: size classes of 16..2048 bytes carved from
   pages, larger requests as whole blocks of pages.  Memory comes from the
   direct map. */
#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

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
  KASSERT(pg->flags & PG_SLAB);
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
