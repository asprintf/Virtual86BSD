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

/* tym.c - typed memory objects (POSIX [TYM]): pools of physically
   contiguous memory set aside at boot, named /dev/tym/NAME (the C
   library's posix_typed_mem_open("/NAME", ...) opens that), and
   /dev/tymctl for posix_mem_offset().  tym=NAME:SIZE[,NAME:SIZE...] on
   the command line (sizes in KiB with K, MiB with M; powers of two pages,
   at most 4 MiB, the allocator's largest block) sets them; the default is
   ram0:4M,ram1:1M.

   How a descriptor maps depends on the flags it was opened with (set by
   ioctl TYM_SETFLAGS right after open):
   - POSIX_TYPED_MEM_ALLOCATE: mmap takes free pages of the pool (any of
     them) for the mapping; they return to the pool when the last mapping
     of them goes;
   - POSIX_TYPED_MEM_ALLOCATE_CONTIG: the same, as one contiguous run;
   - neither: mmap maps the pool at the offset given, and those pages
     cannot be allocated while so mapped, unless the descriptor has
     POSIX_TYPED_MEM_MAP_ALLOCATABLE.
   Mappings are shared memory objects (mm.h) whose pages are the pool's;
   forked processes share them.  New allocations are zeroed. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>

#define TYM_MAJOR 240
#define TYM_CTL_MINOR 255
#define ALLOCATE 1
#define ALLOCATE_CONTIG 2
#define MAP_ALLOCATABLE 4

#define TYM_SETFLAGS 0x54d1
#define TYM_GETINFO 0x54d2
#define TYM_OFFSET 0x54d3

#define NPOOL 8
static struct pool {
  char name[24];
  uint64_t base; /* physical */
  size_t npages;
  uint8_t *alloc;   /* per page: allocated */
  uint16_t *pinned; /* per page: mappings that keep it from allocation */
} pools[NPOOL];
static int npools;

static bool page_free(struct pool *p, size_t i) { return !p->alloc[i] && !p->pinned[i]; }

/* ---- the mappings' memory objects ---- */

struct tymmap {
  struct pool *p;
  bool allocated; /* else pinned (or neither, for MAP_ALLOCATABLE) */
  bool pinned;
};

static void tym_release(struct shmobj *o) {
  struct tymmap *m = o->priv;
  for (size_t k = 0; k < o->npages; k++) {
    size_t i = (size_t)(((o->page[k] & SHM_ADDR) - m->p->base) >> 12);
    if (m->allocated)
      m->p->alloc[i] = 0;
    if (m->pinned)
      m->p->pinned[i]--;
  }
  kfree(m);
}

static int tym_mmap(struct file *f, struct vma *v, uint64_t off, uint64_t len) {
  struct pool *p = &pools[MINOR(f->ino->rdev)];
  int fl = (int)(uintptr_t)f->priv;
  size_t n = (size_t)(PAGE_UP(len) >> 12);
  struct tymmap *m = kzalloc(sizeof(*m));
  struct shmobj *o = shmobj_new(n);
  if (!m || !o) {
    kfree(m);
    if (o)
      shmobj_put(o);
    return -ENOMEM;
  }
  m->p = p;
  if (fl & (ALLOCATE | ALLOCATE_CONTIG)) {
    size_t got = 0;
    if (fl & ALLOCATE_CONTIG) {
      size_t run = 0, at = 0;
      for (size_t i = 0; i < p->npages && run < n; i++) {
        run = page_free(p, i) ? run + 1 : 0;
        if (run == 1)
          at = i;
      }
      if (run >= n)
        for (size_t k = 0; k < n; k++)
          o->page[k] = p->base + ((uint64_t)(at + k) << 12), got++;
    } else
      for (size_t i = 0; i < p->npages && got < n; i++)
        if (page_free(p, i))
          o->page[got++] = p->base + ((uint64_t)i << 12);
    if (got < n) {
      /* (no page was taken yet: o->page holds only candidates, and
         shmobj_put would page_put each of them) */
      memset(o->page, 0, n * sizeof(uint64_t));
      shmobj_put(o);
      kfree(m);
      return -ENOMEM;
    }
    for (size_t k = 0; k < n; k++) {
      p->alloc[(o->page[k] - p->base) >> 12] = 1;
      memset(P2V(o->page[k]), 0, PAGE_SIZE);
    }
    m->allocated = true;
  } else {
    if (off + PAGE_UP(len) > (uint64_t)p->npages << 12 || off + len < off)
      return shmobj_put(o), kfree(m), -ENXIO;
    for (size_t k = 0; k < n; k++)
      o->page[k] = p->base + off + ((uint64_t)k << 12);
    if (!(fl & MAP_ALLOCATABLE)) {
      m->pinned = true;
      for (size_t k = 0; k < n; k++)
        p->pinned[(off >> 12) + k]++;
    }
  }
  for (size_t k = 0; k < n; k++)
    page_get(o->page[k]);
  o->priv = m;
  o->release = tym_release;
  v->shm = o;
  v->shm_off = 0;
  return 0;
}

/* ---- the devices ---- */

static size_t largest_run(struct pool *p) {
  size_t best = 0, run = 0;
  for (size_t i = 0; i < p->npages; i++) {
    run = page_free(p, i) ? run + 1 : 0;
    if (run > best)
      best = run;
  }
  return best;
}

static long mem_offset(uint64_t arg) {
  struct { uint64_t addr, len; int64_t off; uint64_t contig; int32_t fd, pad; } q;
  if (copy_from_user(&q, (void *)arg, sizeof(q)))
    return -EFAULT;
  struct mm *mm = cur_proc()->mm;
  struct vma *v = vma_find(mm, q.addr);
  if (!v || !v->shm || v->shm->release != tym_release)
    return -EACCES; /* not typed memory */
  struct shmobj *o = v->shm;
  struct tymmap *m = o->priv;
  size_t k = v->shm_off + (size_t)((q.addr - v->start) >> 12);
  uint64_t phys = o->page[k] & SHM_ADDR;
  q.off = (int64_t)(phys - m->p->base + (q.addr & (PAGE_SIZE - 1)));
  /* how far the typed memory goes on contiguously from addr */
  uint64_t c = PAGE_SIZE - (q.addr & (PAGE_SIZE - 1));
  for (size_t j = k + 1; c < q.len && j < o->npages && q.addr - (q.addr & (PAGE_SIZE - 1)) + (j - k) * PAGE_SIZE < v->end &&
                         (o->page[j] & SHM_ADDR) == phys + (j - k) * PAGE_SIZE;
       j++)
    c += PAGE_SIZE;
  q.contig = MIN(c, q.len);
  /* a descriptor of this process for the typed memory object */
  q.fd = -1;
  struct proc *pr = cur_proc();
  dev_t want = MKDEV(TYM_MAJOR, (unsigned)(m->p - pools));
  for (int i = 0; i < pr->nfd; i++)
    if (pr->fd[i] && pr->fd[i]->ino && S_ISCHR(pr->fd[i]->ino->mode) && pr->fd[i]->ino->rdev == want) {
      q.fd = i;
      break;
    }
  return copy_to_user((void *)arg, &q, sizeof(q)) ? -EFAULT : 0;
}

static long tym_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  unsigned mi = MINOR(f->ino->rdev);
  if (cmd == TYM_OFFSET)
    return mem_offset(arg);
  if (mi >= (unsigned)npools)
    return -ENOTTY;
  struct pool *p = &pools[mi];
  int fl = (int)(uintptr_t)f->priv;
  if (cmd == TYM_SETFLAGS) {
    if (arg & ~7UL || ((arg & ALLOCATE) && (arg & ALLOCATE_CONTIG)) ||
        ((arg & MAP_ALLOCATABLE) && (arg & (ALLOCATE | ALLOCATE_CONTIG))))
      return -EINVAL;
    f->priv = (void *)(uintptr_t)arg;
    return 0;
  }
  if (cmd == TYM_GETINFO) { /* struct posix_typed_mem_info { size_t posix_tmi_length; } */
    uint64_t len;
    if (fl & ALLOCATE_CONTIG)
      len = (uint64_t)largest_run(p) << 12;
    else if (fl & ALLOCATE) {
      size_t n = 0;
      for (size_t i = 0; i < p->npages; i++)
        n += page_free(p, i);
      len = (uint64_t)n << 12;
    } else
      len = (uint64_t)p->npages << 12;
    return copy_to_user((void *)arg, &len, 8) ? -EFAULT : 0;
  }
  return -ENOTTY;
}

static const struct file_ops tym_fops = {.ioctl = tym_ioctl, .mmap = tym_mmap};
static const struct file_ops tymctl_fops = {.ioctl = tym_ioctl};

static int tym_open(struct inode *ino, struct file *f) {
  unsigned mi = MINOR(ino->rdev);
  if (mi == TYM_CTL_MINOR)
    f->fops = &tymctl_fops;
  else if (mi < (unsigned)npools)
    f->fops = &tym_fops;
  else
    return -ENXIO;
  f->priv = 0;
  return 0;
}

static const struct chrdev tym_dev = {"tym", tym_open};

static uint64_t parse_size(const char **sp) {
  const char *s = *sp;
  uint64_t v = 0;
  while (*s >= '0' && *s <= '9')
    v = v * 10 + (uint64_t)(*s++ - '0');
  if (*s == 'K' || *s == 'k')
    v <<= 10, s++;
  else if (*s == 'M' || *s == 'm')
    v <<= 20, s++;
  *sp = s;
  return v;
}

void tym_init(const char *conf) {
  if (!*conf)
    conf = "ram0:4M,ram1:1M";
  if (!strcmp(conf, "off"))
    return;
  const char *s = conf;
  while (*s && npools < NPOOL) {
    struct pool *p = &pools[npools];
    size_t k = 0;
    while (*s && *s != ':' && *s != ',' && k < sizeof(p->name) - 1)
      p->name[k++] = *s++;
    p->name[k] = 0;
    uint64_t size = *s == ':' ? (s++, parse_size(&s)) : 0;
    while (*s && *s != ',')
      s++;
    if (*s == ',')
      s++;
    int order = 0;
    while ((PAGE_SIZE << order) < size)
      order++;
    if (!k || !size || order > 10) {
      printf("tym: %s: size not supported (at most 4 MiB)\n", p->name);
      p->name[0] = 0;
      continue;
    }
    p->base = alloc_pages(order);
    p->npages = 1UL << order;
    p->alloc = kzalloc(p->npages);
    p->pinned = kzalloc(p->npages * sizeof(uint16_t));
    if (!p->base || !p->alloc || !p->pinned) {
      printf("tym: no memory for %s\n", p->name);
      kfree(p->alloc), kfree(p->pinned);
      if (p->base)
        free_pages(p->base, order);
      memset(p, 0, sizeof(*p));
      continue;
    }
    npools++;
  }
  chrdev_register(TYM_MAJOR, &tym_dev);
}

/* what there is, as one line of dmesg: tym0: ram0 4096KB at 0x3f800000, ... */
void tym_report(void) {
  if (!npools)
    return;
  printf("tym0:");
  for (int i = 0; i < npools; i++)
    printf("%s %s %luKB at 0x%lx", i ? "," : "", pools[i].name, (unsigned long)(pools[i].npages * 4), pools[i].base);
  printf("\n");
}

void tym_populate(void) {
  char path[40];
  if (!npools)
    return;
  for (int i = 0; i < npools; i++) {
    snprintf(path, sizeof(path), "tym/%s", pools[i].name);
    devtmpfs_add(path, S_IFCHR | 0666, MKDEV(TYM_MAJOR, (unsigned)i));
  }
  devtmpfs_add("tymctl", S_IFCHR | 0666, MKDEV(TYM_MAJOR, TYM_CTL_MINOR));
}
