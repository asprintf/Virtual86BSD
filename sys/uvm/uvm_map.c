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

/* vm.c - page tables, address spaces, VMAs, page faults, user access. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

uint64_t kernel_pml4;
extern uint64_t boot_pml4[512];

/* the direct map beyond the first GiB (boot.s mapped that much) */
static void map_direct(void) {
  uint64_t *pml4 = P2V(kernel_pml4);
  uint64_t *pdpt = P2V(pml4[273] & PTE_ADDR);
  uint64_t gib = (max_pfn * PAGE_SIZE + (1UL << 30) - 1) >> 30;
  for (uint64_t g = 1; g < gib && g < 512; g++) {
    if (pdpt[g] & PTE_P)
      continue;
    uint64_t pd = alloc_zeroed_page();
    if (!pd)
      panic("no memory for the direct map");
    uint64_t *e = P2V(pd);
    for (int i = 0; i < 512; i++)
      e[i] = ((g << 30) + ((uint64_t)i << 21)) | PTE_P | PTE_W | PTE_PS | PTE_G | PTE_NX;
    pdpt[g] = pd | PTE_P | PTE_W;
  }
}

void vm_init(void) {
  kernel_pml4 = (uint64_t)boot_pml4 - KERNEL_BASE;
  uint64_t *pml4 = P2V(kernel_pml4);
  pml4[0] = 0; /* the identity map was for the switch to long mode */
  /* every kernel PML4 entry exists now, so that address spaces can share them */
  for (int i = 256; i < 512; i++)
    if (!(pml4[i] & PTE_P))
      pml4[i] = alloc_zeroed_page() | PTE_P | PTE_W;
  write_cr3(kernel_pml4);
  map_direct();
  write_cr3(kernel_pml4);
}

void flush_tlb(void) { write_cr3(read_cr3()); }

uint64_t *pte_get(uint64_t pml4, uint64_t va, bool create) {
  uint64_t *t = P2V(pml4);
  for (int level = 3; level > 0; level--) {
    int idx = (va >> (12 + 9 * level)) & 511;
    if (!(t[idx] & PTE_P)) {
      if (!create)
        return NULL;
      uint64_t p = alloc_zeroed_page();
      if (!p)
        return NULL;
      t[idx] = p | PTE_P | PTE_W | PTE_U;
    }
    t = P2V(t[idx] & PTE_ADDR);
  }
  return &t[(va >> 12) & 511];
}

/* device memory: mapped uncached at kernel addresses of its own (from
   0xffffc90000000000, in a PML4 entry every address space shares) */
void *ioremap(uint64_t phys, size_t size) {
  static uint64_t next = 0xffffc90000000000UL;
  uint64_t off = phys & (PAGE_SIZE - 1);
  phys -= off;
  size = PAGE_UP(size + off);
  uint64_t va = next;
  next += size + PAGE_SIZE; /* (a gap after each) */
  for (uint64_t o = 0; o < size; o += PAGE_SIZE) {
    uint64_t *pte = pte_get(kernel_pml4, va + o, true);
    if (!pte)
      return NULL;
    *pte = (phys + o) | PTE_P | PTE_W | PTE_PCD | PTE_PWT | PTE_G | PTE_NX;
    invlpg(va + o);
  }
  return (void *)(va + off);
}

uint64_t vm_prot_pte(unsigned vmflags) {
  uint64_t f = PTE_P | PTE_U;
  if (vmflags & VM_WRITE)
    f |= PTE_W;
  if (!(vmflags & VM_EXEC))
    f |= PTE_NX;
  return f;
}

int map_page(struct mm *mm, uint64_t va, uint64_t phys, uint64_t flags) {
  uint64_t *pte = pte_get(mm->pml4, va, true);
  if (!pte)
    return -ENOMEM;
  if (*pte & PTE_P)
    page_put(*pte & PTE_ADDR);
  else
    mm->rss++;
  *pte = phys | flags;
  invlpg(va);
  return 0;
}

static void free_table(uint64_t t, int level, uint64_t base, uint64_t start, uint64_t end) {
  uint64_t *e = P2V(t);
  uint64_t span = 1UL << (12 + 9 * level);
  for (int i = 0; i < 512; i++) {
    uint64_t a = base + i * span;
    if (a + span <= start || a >= end || !(e[i] & PTE_P))
      continue;
    if (level == 0) {
      page_put(e[i] & PTE_ADDR);
      e[i] = 0;
    } else {
      free_table(e[i] & PTE_ADDR, level - 1, a, start, end);
      if (a >= start && a + span <= end) {
        free_pages(e[i] & PTE_ADDR, 0);
        e[i] = 0;
      }
    }
  }
}

/* drop the pages of [start, end) (page aligned) */
void unmap_range(struct mm *mm, uint64_t start, uint64_t end) {
  for (uint64_t a = start; a < end;) {
    uint64_t *pde = NULL;
    /* skip quickly over missing upper levels */
    uint64_t *t = P2V(mm->pml4);
    int level;
    for (level = 3; level > 0; level--) {
      int idx = (a >> (12 + 9 * level)) & 511;
      if (!(t[idx] & PTE_P))
        break;
      t = P2V(t[idx] & PTE_ADDR);
    }
    if (level > 0) {
      uint64_t span = 1UL << (12 + 9 * level);
      a = (a & ~(span - 1)) + span;
      continue;
    }
    pde = &t[(a >> 12) & 511];
    if (*pde & PTE_P) {
      page_put(*pde & PTE_ADDR);
      *pde = 0;
      mm->rss--;
      if (mm == (current ? current->proc->mm : NULL))
        invlpg(a);
    }
    a += PAGE_SIZE;
  }
}

struct mm *mm_new(void) {
  struct mm *mm = kzalloc(sizeof(*mm));
  if (!mm)
    return NULL;
  mm->pml4 = alloc_zeroed_page();
  if (!mm->pml4) {
    kfree(mm);
    return NULL;
  }
  uint64_t *n = P2V(mm->pml4), *k = P2V(kernel_pml4);
  for (int i = 256; i < 512; i++)
    n[i] = k[i];
  mm->ref = 1;
  list_init(&mm->vmas);
  mm->mmap_base = 0x7ffff7fff000UL;
  return mm;
}

static void vma_release(struct vma *v) {
  if (v->shm) {
    struct shmobj *o = v->shm;
    shmobj_put(o);
    if (o->owner)
      pcache_unmapped(o); /* (the file keeps the inode until below) */
  }
  if (v->pc) {
    struct shmobj *o = v->pc;
    shmobj_put(o);
    if (o->owner)
      pcache_unmapped(o);
  }
  if (v->file)
    file_put(v->file);
  if (v->shmid >= 0)
    shm_detach_vma(v->shmid);
  kfree(v);
}

void mm_put(struct mm *mm) {
  if (--mm->ref > 0)
    return;
  if (read_cr3() == mm->pml4)
    write_cr3(kernel_pml4);
  list_for_each_safe(p, n, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    list_del(&v->link);
    vma_release(v);
  }
  free_table(mm->pml4, 3, 0, 0, USER_TOP);
  free_pages(mm->pml4, 0);
  kfree(mm);
}

void mm_switch(struct mm *mm) {
  uint64_t p = mm ? mm->pml4 : kernel_pml4;
  if (read_cr3() != p)
    write_cr3(p);
}

static struct vma *vma_copy(struct vma *v) {
  struct vma *n = kmalloc(sizeof(*n));
  if (!n)
    return NULL;
  *n = *v;
  list_init(&n->link);
  if (n->file)
    file_get(n->file);
  if (n->shm)
    shmobj_get(n->shm);
  if (n->pc)
    shmobj_get(n->pc);
  if (n->shmid >= 0)
    shm_attach_vma(n->shmid);
  return n;
}

/* fork: share pages; private writable ones become copy on write */
struct mm *mm_dup(struct mm *old) {
  struct mm *mm = mm_new();
  if (!mm)
    return NULL;
  mm->brk_start = old->brk_start;
  mm->brk = old->brk;
  mm->mmap_base = old->mmap_base;
  mm->stack_top = old->stack_top;
  mm->arg_start = old->arg_start;
  mm->arg_end = old->arg_end;
  mm->env_start = old->env_start;
  mm->env_end = old->env_end;
  list_for_each(p, &old->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    struct vma *n = vma_copy(v);
    if (n)
      n->flags &= ~(unsigned)VM_LOCKED; /* memory locks are not inherited */
    if (!n)
      goto fail;
    list_add_tail(&mm->vmas, &n->link);
    mm->nvmas++;
    for (uint64_t a = v->start; a < v->end; a += PAGE_SIZE) {
      uint64_t *pte = pte_get(old->pml4, a, false);
      if (!pte || !(*pte & PTE_P))
        continue;
      uint64_t e = *pte;
      if (!(v->flags & VM_SHARED) && (e & (PTE_W | PTE_COW))) {
        e = (e & ~PTE_W) | PTE_COW;
        *pte = e;
      }
      uint64_t *npte = pte_get(mm->pml4, a, true);
      if (!npte)
        goto fail;
      page_get(e & PTE_ADDR);
      *npte = e;
      mm->rss++;
    }
  }
  flush_tlb();
  return mm;
fail:
  flush_tlb();
  mm_put(mm);
  return NULL;
}

/* ---- VMAs ---- */

struct vma *vma_find(struct mm *mm, uint64_t addr) {
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (addr < v->start)
      return NULL;
    if (addr < v->end)
      return v;
  }
  return NULL;
}

/* insert a new VMA (the range must be free) */
struct vma *vma_new(struct mm *mm, uint64_t start, uint64_t end, unsigned flags) {
  struct vma *v = kzalloc(sizeof(*v));
  if (!v)
    return NULL;
  v->start = start;
  v->end = end;
  v->flags = flags;
  v->shmid = -1;
  struct list *pos = &mm->vmas;
  list_for_each(p, &mm->vmas) {
    if (list_entry(p, struct vma, link)->start > start) {
      pos = p;
      break;
    }
  }
  list_add_tail(pos, &v->link); /* before pos */
  mm->nvmas++;
  if (start == mm->alloc_lo)
    mm->alloc_hint = v;
  return v;
}

/* split v at addr (inside it): v keeps [start, addr), returns [addr, end) */
static struct vma *vma_split(struct mm *mm, struct vma *v, uint64_t addr) {
  struct vma *n = vma_copy(v);
  if (!n)
    return NULL;
  mm->nvmas++;
  n->start = addr;
  if (n->file)
    n->offset += addr - v->start;
  if (n->shm)
    n->shm_off += (addr - v->start) >> 12;
  v->end = addr;
  list_add(&v->link, &n->link); /* after v */
  return n;
}

int vma_unmap(struct mm *mm, uint64_t start, uint64_t end) {
  list_for_each_safe(p, nx, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= start || v->start >= end)
      continue;
    if (v->start < start) {
      if (!vma_split(mm, v, start))
        return -ENOMEM;
      continue; /* the new piece comes next */
    }
    if (v->end > end) {
      if (!vma_split(mm, v, end))
        return -ENOMEM;
    }
    unmap_range(mm, v->start, v->end);
    list_del(&v->link);
    mm->nvmas--;
    if (mm->alloc_hint == v)
      mm->alloc_hint = NULL;
    vma_release(v);
  }
  return 0;
}

static bool range_free(struct mm *mm, uint64_t start, uint64_t end) {
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->start < end && v->end > start)
      return false;
  }
  return true;
}

/* find room for len bytes, top down from mmap_base (as Linux) */
uint64_t vma_free_area(struct mm *mm, uint64_t len, uint64_t hint) {
  if (hint && hint + len <= USER_TOP - PAGE_SIZE && hint >= 0x10000 &&
      range_free(mm, hint, hint + len))
    return hint;
  /* the common case first: right below the mapping placed last, when
     the VMA before it (the list is sorted) leaves room */
  struct vma *h = mm->alloc_hint;
  if (h && h->start <= mm->mmap_base && h->start >= len + 0x10000) {
    uint64_t start = h->start - len;
    struct list *pp = h->link.prev;
    if (pp == &mm->vmas || list_entry(pp, struct vma, link)->end <= start)
      return mm->alloc_lo = start;
  }
  /* else the highest gap below mmap_base: the VMAs from the top down */
  uint64_t top = mm->mmap_base;
  for (struct list *p = mm->vmas.prev; p != &mm->vmas; p = p->prev) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->start >= top)
      continue;
    if (v->end < top && top - v->end >= len)
      break;
    top = v->start;
  }
  if (top < len + 0x10000)
    return 0;
  return mm->alloc_lo = top - len;
}

/* ---- faults ---- */

static int fault_in(struct mm *mm, struct vma *v, uint64_t addr) {
  uint64_t page = PAGE_DOWN(addr);
  uint64_t flags = vm_prot_pte(v->flags);
  uint64_t phys;
  if (v->shm) {
    size_t idx = v->shm_off + ((page - v->start) >> 12);
    if (idx >= v->shm->npages)
      return -EFAULT; /* beyond the end of the object: SIGBUS */
    phys = shmobj_page(v->shm, idx);
    if (!phys)
      return -ENOMEM;
    if (v->shm->owner && idx < v->shm->npages)
      v->shm->page[idx] |= SHM_CAND; /* a page cache page a mapping may change */
    page_get(phys);
    return map_page(mm, page, phys, flags);
  }
  if (v->file && !(v->flags & VM_SHARED) && !(v->offset & (PAGE_SIZE - 1))) {
    /* a private file mapping maps the page cache's page until it is
       written (copy on write, also where the mapping is not writable now:
       mprotect may make it so), so that every process running a program
       or a library shares its pages (and QEMU need not translate the same
       code again for each) */
    struct inode *ino = v->file->ino;
    uint64_t off = v->offset + (page - v->start);
    if (!v->pc && ino && (ino->mode & S_IFMT) == S_IFREG)
      v->pc = pcache_get(ino);
    if (v->pc) {
      if ((off >> 12) >= v->pc->npages)
        return -EFAULT; /* wholly beyond the end of the file: SIGBUS */
      phys = shmobj_page(v->pc, (size_t)(off >> 12));
      if (!phys)
        return -ENOMEM;
      page_get(phys);
      return map_page(mm, page, phys, (flags & ~PTE_W) | PTE_COW);
    }
  }
  phys = alloc_page();
  if (!phys)
    return -ENOMEM;
  memset(P2V(phys), 0, PAGE_SIZE);
  if (v->file) {
    uint64_t off = v->offset + (page - v->start);
    ssize_t n = kernel_read(v->file, P2V(phys), PAGE_SIZE, (off_t)off);
    if (n < 0) {
      page_put(phys);
      return (int)n;
    }
  }
  if (!(v->flags & VM_SHARED) && (flags & PTE_W) == 0 && v->file)
    flags |= 0; /* read-only private file page */
  return map_page(mm, page, phys, flags);
}

static int do_cow(struct mm *mm, uint64_t *pte, uint64_t addr) {
  uint64_t old = *pte & PTE_ADDR;
  if (phys_page(old)->ref == 1) {
    *pte = (*pte | PTE_W) & ~PTE_COW;
  } else {
    uint64_t n = alloc_page();
    if (!n)
      return -ENOMEM;
    memcpy(P2V(n), P2V(old), PAGE_SIZE);
    *pte = n | ((*pte | PTE_W) & ~PTE_COW & ~PTE_ADDR);
    page_put(old);
  }
  (void)mm;
  invlpg(PAGE_DOWN(addr));
  return 0;
}

/* resolve a fault at addr: 0, or -EFAULT (no mapping / not allowed),
   -ENOMEM; -EBUSY for SIGBUS */
int handle_fault(struct mm *mm, uint64_t addr, bool write, bool user) {
  (void)user;
  if (!mm || addr >= USER_TOP)
    return -EFAULT;
  struct vma *v = vma_find(mm, addr);
  if (!v) {
    /* grow a stack downwards */
    list_for_each(p, &mm->vmas) {
      struct vma *s = list_entry(p, struct vma, link);
      if (s->start > addr) {
        if ((s->flags & VM_GROWSDOWN) && s->start - PAGE_DOWN(addr) <= 64UL << 20) {
          struct list *pr = s->link.prev;
          if (pr != &mm->vmas && list_entry(pr, struct vma, link)->end > PAGE_DOWN(addr) - PAGE_SIZE)
            return -EFAULT;
          s->start = PAGE_DOWN(addr);
          v = s;
        }
        break;
      }
    }
    if (!v)
      return -EFAULT;
  }
  if (write && !(v->flags & VM_WRITE))
    return -EFAULT;
  if (!write && !(v->flags & (VM_READ | VM_WRITE | VM_EXEC)))
    return -EFAULT;
  uint64_t *pte = pte_get(mm->pml4, addr, true);
  if (!pte)
    return -ENOMEM;
  if (*pte & PTE_P) {
    if (write && (*pte & PTE_COW))
      return do_cow(mm, pte, addr);
    if (write && !(*pte & PTE_W))
      return -EFAULT;
    invlpg(PAGE_DOWN(addr)); /* stale TLB entry */
    return 0;
  }
  int r = fault_in(mm, v, addr);
  if (r == 0 && write) {
    pte = pte_get(mm->pml4, addr, false);
    if (pte && (*pte & PTE_COW))
      r = do_cow(mm, pte, addr);
  }
  return r;
}

void page_fault(struct regs *r) {
  uint64_t addr = read_cr2();
  bool write = r->err & 2;
  bool user = (r->cs & 3) == 3;
  struct mm *mm = current && current->proc ? current->proc->mm : NULL;
  int e = handle_fault(mm, addr, write, user);
  if (e == 0)
    return;
  if (user) {
    struct vma *v = mm ? vma_find(mm, addr) : NULL;
    if (e == -ENOMEM)
      force_signal(SIGKILL, SI_KERNEL, addr, 14, r->err);
    else if (v && (v->shm || v->pc) && e == -EFAULT &&
             (v->flags & (write ? VM_WRITE : VM_READ | VM_WRITE | VM_EXEC)))
      /* allowed by the protection, beyond the end of the object */
      force_signal(SIGBUS, BUS_ADRERR, addr, 14, r->err);
    else {
      struct proc *p = cur_proc();
      if (p->sigact[SIGSEGV - 1].handler == SIG_DFL)
        printf("%s[%d]: segfault at %lx ip %lx sp %lx error %lx\n", p->comm, current->tid, addr, r->rip,
               r->rsp, r->err);
      force_signal(SIGSEGV, v ? SEGV_ACCERR : SEGV_MAPERR, addr, 14, r->err);
    }
    return;
  }
  if (r->rip >= (uint64_t)user_access_begin && r->rip < (uint64_t)user_access_end) {
    r->rip = (uint64_t)user_access_fault;
    return;
  }
  kernel_fault(r, addr);
}

/* make [start, end) present (mlock, MAP_POPULATE, before kernel writes) */
int mm_populate(struct mm *mm, uint64_t start, uint64_t end, bool write) {
  for (uint64_t a = PAGE_DOWN(start); a < end; a += PAGE_SIZE) {
    uint64_t *pte = pte_get(mm->pml4, a, false);
    if (pte && (*pte & PTE_P) && (!write || (*pte & PTE_W)))
      continue;
    int e = handle_fault(mm, a, write, false);
    if (e)
      return e;
  }
  return 0;
}

/* ---- user memory ---- */

bool user_range_ok(uint64_t addr, size_t n) {
  return addr <= USER_TOP && n <= USER_TOP - addr;
}

int copy_from_user(void *dst, const void *usrc, size_t n) {
  if ((uint64_t)usrc >= USER_TOP) { /* kernel buffer (internal callers) */
    memcpy(dst, usrc, n);
    return 0;
  }
  if (!user_range_ok((uint64_t)usrc, n))
    return -EFAULT;
  return user_copy(dst, usrc, n) ? -EFAULT : 0;
}

int copy_to_user(void *udst, const void *src, size_t n) {
  if ((uint64_t)udst >= USER_TOP) {
    memcpy(udst, src, n);
    return 0;
  }
  if (!user_range_ok((uint64_t)udst, n))
    return -EFAULT;
  return user_copy(udst, src, n) ? -EFAULT : 0;
}

int clear_user(void *udst, size_t n) {
  if ((uint64_t)udst >= USER_TOP) {
    memset(udst, 0, n);
    return 0;
  }
  if (!user_range_ok((uint64_t)udst, n))
    return -EFAULT;
  return user_set(udst, 0, n) ? -EFAULT : 0;
}

/* copy a string of at most max-1 bytes: its length, -EFAULT, or
   -ENAMETOOLONG if it does not fit */
long strncpy_from_user(char *dst, const char *usrc, size_t max) {
  long n;
  if ((uint64_t)usrc >= USER_TOP)
    n = (long)strnlen(usrc, max);
  else {
    if ((uint64_t)usrc >= USER_TOP)
      return -EFAULT;
    size_t lim = MIN(max, USER_TOP - (uint64_t)usrc);
    n = user_strnlen(usrc, lim);
    if (n < 0)
      return -EFAULT;
  }
  if ((size_t)n >= max)
    return -ENAMETOOLONG;
  if (copy_from_user(dst, usrc, (size_t)n))
    return -EFAULT;
  dst[n] = 0;
  return n;
}

char *user_path(const char *upath) {
  char *buf = kmalloc(4096);
  if (!buf)
    return ERR_PTR(-ENOMEM);
  long n = strncpy_from_user(buf, upath, 4096);
  if (n < 0) {
    kfree(buf);
    return ERR_PTR(n);
  }
  return buf;
}

/* ---- shared memory objects ---- */

struct shmobj *shmobj_new(size_t npages) {
  struct shmobj *o = kzalloc(sizeof(*o));
  if (!o)
    return NULL;
  o->ref = 1;
  if (shmobj_resize(o, npages)) {
    kfree(o);
    return NULL;
  }
  return o;
}

void shmobj_get(struct shmobj *o) { o->ref++; }

int shmobj_resize(struct shmobj *o, size_t npages) {
  if (npages < o->npages) {
    for (size_t i = npages; i < o->npages; i++)
      if (o->page[i])
        page_put(o->page[i] & SHM_ADDR);
    o->npages = npages;
    return 0;
  }
  if (npages == o->npages)
    return 0;
  uint64_t *np = kzalloc(npages * sizeof(uint64_t) + 8);
  if (!np)
    return -ENOMEM;
  if (o->page)
    memcpy(np, o->page, o->npages * sizeof(uint64_t));
  kfree(o->page);
  o->page = np;
  o->npages = npages;
  return 0;
}

void shmobj_put(struct shmobj *o) {
  if (--o->ref > 0)
    return;
  if (o->release)
    o->release(o);
  shmobj_resize(o, 0);
  kfree(o->page);
  kfree(o);
}

uint64_t shmobj_page(struct shmobj *o, size_t idx) {
  if (idx >= o->npages)
    return 0;
  if (!o->page[idx]) {
    if (o->owner)
      return pcache_fill(o, idx);
    o->page[idx] = alloc_zeroed_page();
  }
  return o->page[idx] & SHM_ADDR;
}

void shmobj_unmap_from(struct shmobj *o, size_t idx) {
  list_for_each(p, &all_procs) {
    struct mm *mm = list_entry(p, struct proc, all_link)->mm;
    if (!mm)
      continue;
    list_for_each(q, &mm->vmas) {
      struct vma *v = list_entry(q, struct vma, link);
      size_t first;
      if (v->pc == o && !(v->flags & VM_SHARED))
        first = (size_t)(v->offset >> 12); /* a private mapping: copied pages go too, as on Linux */
      else if (v->shm == o)
        first = v->shm_off;
      else
        continue;
      uint64_t from = idx > first ? v->start + ((uint64_t)(idx - first) << 12) : v->start;
      if (from < v->end)
        unmap_range(mm, from, v->end);
    }
  }
}

/* split the VMA containing addr there; the part from addr on */
struct vma *vma_split_at(struct mm *mm, uint64_t addr) {
  struct vma *v = vma_find(mm, addr);
  if (!v)
    return NULL;
  if (v->start == addr)
    return v;
  return vma_split(mm, v, addr);
}

void vma_protect_range(struct mm *mm, uint64_t start, uint64_t end, unsigned prot) {
  vma_split_at(mm, start);
  vma_split_at(mm, end);
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= start || v->start >= end)
      continue;
    v->flags = (v->flags & ~(unsigned)(VM_READ | VM_WRITE | VM_EXEC)) | prot;
    for (uint64_t a = v->start; a < v->end; a += PAGE_SIZE) {
      uint64_t *pte = pte_get(mm->pml4, a, false);
      if (!pte || !(*pte & PTE_P))
        continue;
      uint64_t e = (*pte & ~(PTE_W | PTE_NX)) | PTE_U;
      if (!(prot & VM_EXEC))
        e |= PTE_NX;
      /* a private page others map too (after fork, or the page cache's)
         becomes copy on write, not writable */
      if ((prot & VM_WRITE) && !(e & PTE_COW)) {
        if (!(v->flags & VM_SHARED) && phys_page(e & PTE_ADDR)->ref > 1)
          e |= PTE_COW;
        else
          e |= PTE_W;
      }
      if (!(prot & (VM_READ | VM_WRITE | VM_EXEC)))
        e &= ~PTE_U; /* PROT_NONE: the page stays, user access faults */
      *pte = e;
      invlpg(a);
    }
  }
}
