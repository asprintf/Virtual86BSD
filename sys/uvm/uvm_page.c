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

/* page.c - physical page allocator: a buddy system over all usable RAM,
   with a reference count per page (copy on write, shared mappings). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define MAX_ORDER 11 /* blocks of up to 2^10 pages = 4 MiB */
#define NIL 0xffffffffU

struct page *pages;
uint64_t max_pfn, total_pages, free_pages_count;
static uint32_t free_head[MAX_ORDER];

static void fl_add(int order, uint32_t pfn) {
  struct page *p = &pages[pfn];
  p->flags = PG_FREE;
  p->order = (uint8_t)order;
  p->prev = NIL;
  p->next = free_head[order];
  if (p->next != NIL)
    pages[p->next].prev = pfn;
  free_head[order] = pfn;
}

static void fl_del(int order, uint32_t pfn) {
  struct page *p = &pages[pfn];
  if (p->prev != NIL)
    pages[p->prev].next = p->next;
  else
    free_head[order] = p->next;
  if (p->next != NIL)
    pages[p->next].prev = p->prev;
  p->flags = 0;
}

static void free_block(uint64_t pfn, int order) {
  while (order < MAX_ORDER - 1) {
    uint64_t buddy = pfn ^ (1UL << order);
    if (buddy >= max_pfn)
      break;
    struct page *b = &pages[buddy];
    if (!(b->flags & PG_FREE) || b->order != order)
      break;
    fl_del(order, (uint32_t)buddy);
    pfn &= ~(1UL << order);
    order++;
  }
  fl_add(order, (uint32_t)pfn);
}

int irq_nesting; /* > 0 in an interrupt handler (arch/x86.c) */

uint64_t alloc_pages(int order) {
  uint64_t f = irq_save();
  int o = order;
  while (o < MAX_ORDER && free_head[o] == NIL)
    o++;
  if (o == MAX_ORDER && !irq_nesting) {
    /* out of memory: the buffer cache gives some back (it may write
       buffers, so not in an interrupt) */
    irq_restore(f);
    bcache_reclaim(256 + (2UL << order));
    f = irq_save();
    for (o = order; o < MAX_ORDER && free_head[o] == NIL;)
      o++;
  }
  if (o == MAX_ORDER) {
    irq_restore(f);
    return 0;
  }
  uint32_t pfn = free_head[o];
  fl_del(o, pfn);
  while (o > order) {
    o--;
    fl_add(o, pfn + (1U << o));
  }
  for (uint64_t i = 0; i < 1UL << order; i++) {
    pages[pfn + i].flags = 0;
    pages[pfn + i].ref = 1;
  }
  pages[pfn].order = (uint8_t)order;
  free_pages_count -= 1UL << order;
  irq_restore(f);
  return (uint64_t)pfn << 12;
}

void free_pages(uint64_t phys, int order) {
  uint64_t f = irq_save();
  uint64_t pfn = phys >> 12;
  for (uint64_t i = 0; i < 1UL << order; i++)
    pages[pfn + i].ref = 0;
  free_pages_count += 1UL << order;
  free_block(pfn, order);
  irq_restore(f);
}

uint64_t alloc_page(void) { return alloc_pages(0); }

uint64_t alloc_zeroed_page(void) {
  uint64_t p = alloc_pages(0);
  if (p)
    memset(P2V(p), 0, PAGE_SIZE);
  return p;
}

void page_get(uint64_t phys) {
  uint64_t f = irq_save();
  pages[phys >> 12].ref++;
  irq_restore(f);
}

void page_put(uint64_t phys) {
  uint64_t f = irq_save();
  struct page *p = &pages[phys >> 12];
  KASSERT(p->ref > 0);
  if (--p->ref == 0) {
    free_pages_count++;
    free_block(phys >> 12, 0);
  }
  irq_restore(f);
}

/* Frees the usable pages of ranges in [lo, hi) */
static void add_pages(uint64_t lo, uint64_t hi, uint64_t arr_end, const uint64_t *ranges,
                      int nranges, const uint64_t *reserved, int nreserved) {
  for (int i = 0; i < nranges; i++) {
    uint64_t s = PAGE_UP(ranges[2 * i]), e = PAGE_DOWN(ranges[2 * i + 1]);
    if (e >> 12 > max_pfn)
      e = max_pfn << 12;
    if (s < lo)
      s = lo;
    if (e > hi)
      e = hi;
    for (uint64_t a = s; a < e; a += PAGE_SIZE) {
      if (a < 0x100000 || a < arr_end)
        continue; /* low memory, kernel image, page array */
      bool res = false;
      for (int k = 0; k < nreserved; k++)
        if (a + PAGE_SIZE > reserved[2 * k] && a < reserved[2 * k + 1])
          res = true;
      if (res)
        continue;
      pages[a >> 12].flags = 0;
      total_pages++;
      free_pages_count++;
      free_block(a >> 12, 0);
    }
  }
}

/* The memory above the first GiB, once the direct map covers it (until
   then, page tables allocated for the direct map itself must be in the
   first GiB, which the boot page tables map) */
void page_init_high(const uint64_t *ranges, int nranges, const uint64_t *reserved,
                    int nreserved) {
  add_pages(1UL << 30, ~0UL, 0, ranges, nranges, reserved, nreserved);
}

/* ranges: pairs [start, end) of usable RAM; reserved: pairs not to use.
   Only the first GiB: page_init_high adds the rest. */
void page_init(uint64_t kernel_end, const uint64_t *ranges, int nranges,
               const uint64_t *reserved, int nreserved) {
  for (int i = 0; i < MAX_ORDER; i++)
    free_head[i] = NIL;
  max_pfn = 0;
  for (int i = 0; i < nranges; i++)
    if (ranges[2 * i + 1] >> 12 > max_pfn)
      max_pfn = ranges[2 * i + 1] >> 12;
  if (max_pfn > (1UL << 32) - 2)
    max_pfn = (1UL << 32) - 2;
  /* the page array goes right after the kernel image */
  uint64_t arr = PAGE_UP(kernel_end);
  uint64_t arr_end = PAGE_UP(arr + max_pfn * sizeof(struct page));
  if (arr_end > (1UL << 30))
    panic("page array does not fit in the first GiB");
  pages = P2V(arr);
  for (uint64_t i = 0; i < max_pfn; i++) {
    pages[i].flags = PG_RESERVED;
    pages[i].ref = 0;
    pages[i].order = 0;
  }
  add_pages(0, 1UL << 30, arr_end, ranges, nranges, reserved, nreserved);
}
