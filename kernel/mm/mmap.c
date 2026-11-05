/* mmap.c - memory mapping system calls. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "dev.h"
#include "syscall.h"

static unsigned prot_flags(int prot) {
  return (prot & PROT_READ ? VM_READ : 0) | (prot & PROT_WRITE ? VM_WRITE : 0) |
         (prot & PROT_EXEC ? VM_EXEC | VM_READ : 0);
}

/* RLIMIT_AS: may the address space grow by more bytes? */
static bool as_room(struct mm *mm, uint64_t more) {
  uint64_t lim = cur_proc()->rlim[RLIMIT_AS].cur;
  if (lim == RLIM_INFINITY)
    return true;
  uint64_t tot = 0;
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    tot += v->end - v->start;
  }
  return tot + more <= lim && tot + more >= tot;
}

long do_mmap(uint64_t addr, uint64_t len, int prot, int flags, struct file *f, uint64_t off) {
  struct mm *mm = cur_proc()->mm;
  int type = flags & MAP_TYPE;
  if (type != MAP_SHARED && type != MAP_PRIVATE && type != MAP_SHARED_VALIDATE)
    return -EINVAL;
  if (len == 0 || (off & 0xfff))
    return -EINVAL;
  if (prot & ~7)
    return -EINVAL;
  len = PAGE_UP(len);
  if (len == 0 || len > USER_TOP)
    return -ENOMEM;
  bool shared = type != MAP_PRIVATE;
  if (!as_room(mm, len) || mm->nvmas >= MAX_MAP_COUNT)
    return -ENOMEM;
  if (f) {
    unsigned acc = f->flags & O_ACCMODE;
    if (acc == O_WRONLY || (f->flags & O_PATH))
      return -EACCES;
    if (shared && (prot & PROT_WRITE) && acc != O_RDWR)
      return -EACCES;
    struct inode *ino = f->ino;
    if (S_ISDIR(ino->mode))
      return -ENODEV;
    if (!S_ISREG(ino->mode) && !S_ISCHR(ino->mode) && !S_ISBLK(ino->mode))
      return -ENODEV;
    if (S_ISCHR(ino->mode) && !ino->data && !(f->fops && f->fops->mmap)) {
      /* /dev/zero: anonymous memory */
      if (MAJOR(ino->rdev) == 1 && MINOR(ino->rdev) == 5)
        f = NULL;
      else
        return -ENODEV;
    }
  }
  uint64_t start;
  if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
    if (addr & 0xfff)
      return -EINVAL;
    if (addr + len > USER_TOP || addr + len < addr)
      return -ENOMEM;
    start = addr;
    if (flags & MAP_FIXED_NOREPLACE) {
      list_for_each(p, &mm->vmas) {
        struct vma *v = list_entry(p, struct vma, link);
        if (v->start < start + len && v->end > start)
          return -EEXIST;
      }
    } else {
      int e = vma_unmap(mm, start, start + len);
      if (e)
        return e;
    }
  } else {
    start = vma_free_area(mm, len, PAGE_DOWN(addr));
    if (!start)
      return -ENOMEM;
  }
  unsigned vf = prot_flags(prot);
  if (shared)
    vf |= VM_SHARED;
  if (flags & MAP_GROWSDOWN)
    vf |= VM_GROWSDOWN;
  struct vma *v = vma_new(mm, start, start + len, vf | (mm->lock_future ? VM_LOCKED : 0));
  if (!v)
    return -ENOMEM;
  if (f && f->fops && f->fops->mmap) { /* a device with memory of its own */
    v->file = f;
    file_get(f);
    v->offset = off;
    int e = f->fops->mmap(f, v, off, len);
    if (e) {
      vma_unmap(mm, start, start + len);
      return e;
    }
  } else if (f) {
    struct inode *ino = f->ino;
    if (shared && ino->data) {
      /* tmpfs: map the file's own pages */
      v->shm = ino->data;
      shmobj_get(ino->data);
      v->shm_off = off >> 12;
    } else if (shared && S_ISREG(ino->mode) && ino->iops && ino->iops->rawio) {
      /* a block file system: the file's page cache (fs/pcache.c) */
      v->shm = pcache_get(ino);
      if (!v->shm) {
        vma_unmap(mm, start, start + len);
        return -ENOMEM;
      }
      v->shm_off = off >> 12;
      v->file = f; /* keeps the inode; /proc/<pid>/maps names it */
      file_get(f);
      v->offset = off;
    } else {
      /* a private copy (shared mappings of devices: not seen by others) */
      v->file = f;
      file_get(f);
      v->offset = off;
      if (shared)
        v->flags &= ~VM_SHARED;
    }
  } else if (shared) {
    v->shm = shmobj_new(len >> 12);
    if (!v->shm) {
      vma_unmap(mm, start, start + len);
      return -ENOMEM;
    }
  }
  if (flags & (MAP_POPULATE | MAP_LOCKED))
    mm_populate(mm, start, start + len, false);
  return (long)start;
}

long sys_mmap(long addr, long len, long prot, long flags, long fd, long off) {
  struct file *f = NULL;
  if (!(flags & MAP_ANONYMOUS)) {
    f = fd_get((int)fd);
    if (!f)
      return -EBADF;
  }
  return do_mmap((uint64_t)addr, (uint64_t)len, (int)prot, (int)flags, f, (uint64_t)off);
}

long sys_munmap(long addr, long len) {
  if ((addr & 0xfff) || len <= 0)
    return -EINVAL;
  uint64_t end = PAGE_UP((uint64_t)addr + (uint64_t)len);
  if (end > USER_TOP || end < (uint64_t)addr)
    return -EINVAL;
  return vma_unmap(cur_proc()->mm, (uint64_t)addr, end);
}

long sys_mprotect(long addr, long len, long prot) {
  if ((addr & 0xfff) || (prot & ~(7 | 0x01000000 | 0x02000000)))
    return -EINVAL;
  struct mm *mm = cur_proc()->mm;
  uint64_t start = (uint64_t)addr, end = PAGE_UP(start + (uint64_t)len);
  if (end < start || end > USER_TOP)
    return -ENOMEM;
  if (start == end)
    return 0;
  /* the whole range must be mapped */
  uint64_t a = start;
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= a)
      continue;
    if (v->start > a)
      break;
    a = v->end;
    if (a >= end)
      break;
  }
  if (a < end)
    return -ENOMEM;
  unsigned nf = prot_flags((int)prot & 7);
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= start || v->start >= end)
      continue;
    if (v->file && (nf & VM_WRITE) && (v->flags & VM_SHARED) && (v->file->flags & O_ACCMODE) != O_RDWR)
      return -EACCES;
  }
  /* split at the edges, then change */
  vma_protect_range(mm, start, end, nf);
  return 0;
}

long sys_brk(long addr) {
  struct mm *mm = cur_proc()->mm;
  uint64_t nb = (uint64_t)addr;
  if (nb < mm->brk_start)
    return (long)mm->brk;
  uint64_t old_end = PAGE_UP(mm->brk), new_end = PAGE_UP(nb);
  if (new_end > old_end) {
    if (!as_room(mm, new_end - old_end))
      return (long)mm->brk;
    list_for_each(p, &mm->vmas) {
      struct vma *v = list_entry(p, struct vma, link);
      if (v->start < new_end && v->end > old_end)
        return (long)mm->brk;
    }
    /* extend the heap VMA or make one */
    struct vma *h = old_end > mm->brk_start ? vma_find(mm, old_end - 1) : NULL;
    if (h && h->end == old_end && !h->file && !h->shm && (h->flags & ~(unsigned)VM_LOCKED) == (VM_READ | VM_WRITE))
      h->end = new_end;
    else if (!vma_new(mm, old_end, new_end, VM_READ | VM_WRITE | (mm->lock_future ? VM_LOCKED : 0)))
      return (long)mm->brk;
  } else if (new_end < old_end)
    vma_unmap(mm, new_end, old_end);
  mm->brk = nb;
  return (long)nb;
}

#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED 2

long sys_mremap(long oaddr, long olen, long nlen, long flags, long naddr) {
  struct mm *mm = cur_proc()->mm;
  uint64_t old = (uint64_t)oaddr;
  if ((old & 0xfff) || (flags & ~3) || ((flags & MREMAP_FIXED) && !(flags & MREMAP_MAYMOVE)))
    return -EINVAL;
  uint64_t os = PAGE_UP((uint64_t)olen), ns = PAGE_UP((uint64_t)nlen);
  if (ns == 0)
    return -EINVAL;
  struct vma *v = vma_find(mm, old);
  if (!v || old + os > v->end)
    return -EFAULT;
  if (flags & MREMAP_FIXED) {
    uint64_t na = (uint64_t)naddr;
    if ((na & 0xfff) || na + ns > USER_TOP)
      return -EINVAL;
    if (na < old + os && old < na + ns)
      return -EINVAL;
  } else {
    if (ns <= os) {
      if (ns < os)
        vma_unmap(mm, old + ns, old + os);
      return (long)old;
    }
    /* grow in place? */
    if (old + os == v->end) {
      bool free = old + ns <= USER_TOP;
      list_for_each(p, &mm->vmas) {
        struct vma *w = list_entry(p, struct vma, link);
        if (w != v && w->start < old + ns && w->end > v->end)
          free = false;
      }
      if (free && !v->shm) {
        v->end = old + ns;
        return (long)old;
      }
    }
    if (!(flags & MREMAP_MAYMOVE))
      return -ENOMEM;
  }
  /* move: a new VMA, page table entries carried over */
  uint64_t na;
  if (flags & MREMAP_FIXED) {
    na = (uint64_t)naddr;
    vma_unmap(mm, na, na + ns);
  } else {
    na = vma_free_area(mm, ns, 0);
    if (!na)
      return -ENOMEM;
  }
  if (old > v->start && !(v = vma_split_at(mm, old)))
    return -ENOMEM;
  if (old + os < v->end && !vma_split_at(mm, old + os))
    return -ENOMEM;
  struct vma *n = vma_new(mm, na, na + ns, v->flags);
  if (!n)
    return -ENOMEM;
  n->file = v->file;
  if (n->file)
    file_get(n->file);
  n->offset = v->offset;
  n->shm = v->shm;
  if (n->shm)
    shmobj_get(n->shm);
  n->shm_off = v->shm_off;
  uint64_t copy = MIN(os, ns);
  for (uint64_t a = 0; a < copy; a += PAGE_SIZE) {
    uint64_t *pte = pte_get(mm->pml4, old + a, false);
    if (!pte || !(*pte & PTE_P))
      continue;
    uint64_t e = *pte;
    *pte = 0;
    invlpg(old + a);
    uint64_t *npte = pte_get(mm->pml4, na + a, true);
    if (!npte)
      return -ENOMEM;
    *npte = e;
    invlpg(na + a);
  }
  vma_unmap(mm, old, old + os);
  return (long)na;
}

long sys_madvise(long addr, long len, long advice) {
  (void)len;
  if (addr & 0xfff)
    return -EINVAL;
  if (advice == 4) { /* MADV_DONTNEED: private anonymous pages read as zero again */
    struct mm *mm = cur_proc()->mm;
    uint64_t s = (uint64_t)addr, e = PAGE_UP(s + (uint64_t)len);
    list_for_each(p, &mm->vmas) {
      struct vma *v = list_entry(p, struct vma, link);
      if (v->end <= s || v->start >= e || (v->flags & VM_SHARED))
        continue;
      unmap_range(mm, MAX(s, v->start), MIN(e, v->end));
    }
  }
  return 0;
}

/* memory locks: all memory stays resident here (there is no swap), so a
   lock is only the VMA's VM_LOCKED, which POSIX lets one see: msync with
   MS_INVALIDATE fails with EBUSY on locked pages.  Locks are not inherited
   by fork (mm_dup) nor kept by exec. */
static void lock_range(struct mm *mm, uint64_t s, uint64_t e, bool lock) {
  vma_split_at(mm, s);
  vma_split_at(mm, e);
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= s || v->start >= e)
      continue;
    if (lock)
      v->flags |= VM_LOCKED;
    else
      v->flags &= ~(unsigned)VM_LOCKED;
  }
}

static int lock_args(long addr, long len, uint64_t *s, uint64_t *e) {
  *s = PAGE_DOWN((uint64_t)addr);
  *e = PAGE_UP((uint64_t)addr + (uint64_t)len);
  if (*e < *s)
    return -ENOMEM;
  struct mm *mm = cur_proc()->mm;
  for (uint64_t a = *s; a < *e; a += PAGE_SIZE)
    if (!vma_find(mm, a))
      return -ENOMEM;
  return 0;
}

long sys_mlock(long addr, long len) {
  uint64_t s, e;
  int r = lock_args(addr, len, &s, &e);
  if (r)
    return r;
  struct mm *mm = cur_proc()->mm;
  mm_populate(mm, s, e, false);
  lock_range(mm, s, e, true);
  return 0;
}
long sys_munlock(long addr, long len) {
  uint64_t s, e;
  int r = lock_args(addr, len, &s, &e);
  if (r)
    return r;
  lock_range(cur_proc()->mm, s, e, false);
  return 0;
}
long sys_mlockall(long flags) {
  if (flags & ~7 || !(flags & 3))
    return -EINVAL;
  struct mm *mm = cur_proc()->mm;
  if (flags & 1) { /* MCL_CURRENT */
    list_for_each(p, &mm->vmas) {
      struct vma *v = list_entry(p, struct vma, link);
      mm_populate(mm, v->start, v->end, false);
      v->flags |= VM_LOCKED;
    }
  }
  mm->lock_future = (flags & 2) != 0; /* MCL_FUTURE */
  return 0;
}
long sys_munlockall(void) {
  struct mm *mm = cur_proc()->mm;
  list_for_each(p, &mm->vmas) list_entry(p, struct vma, link)->flags &= ~(unsigned)VM_LOCKED;
  mm->lock_future = false;
  return 0;
}

long sys_msync(long addr, long len, long flags) {
  if ((addr & 0xfff) || (flags & ~7) || ((flags & 1) && (flags & 4)))
    return -EINVAL;
  struct mm *mm = cur_proc()->mm;
  uint64_t s = (uint64_t)addr, e = PAGE_UP(s + (uint64_t)len);
  for (uint64_t a = s; a < e; a += PAGE_SIZE)
    if (!vma_find(mm, a))
      return -ENOMEM;
  if (flags & 2) /* MS_INVALIDATE of locked pages */
    list_for_each(p, &mm->vmas) {
      struct vma *v = list_entry(p, struct vma, link);
      if (v->end > s && v->start < e && (v->flags & VM_LOCKED))
        return -EBUSY;
    }
  /* write the page caches of the files mapped there back (MS_ASYNC too:
     it is quick), and with MS_SYNC to the disk */
  list_for_each(p, &mm->vmas) {
    struct vma *v = list_entry(p, struct vma, link);
    if (v->end <= s || v->start >= e || !v->shm || !v->shm->owner)
      continue;
    struct inode *i = v->shm->owner;
    int r = pcache_sync(i);
    if (r)
      return r == -ENOSPC ? -EIO : r;
    if ((flags & 4) && v->file && v->file->fops && v->file->fops->fsync)
      v->file->fops->fsync(v->file);
  }
  return 0;
}

long sys_mincore(long addr, long len, long uvec) {
  struct mm *mm = cur_proc()->mm;
  if (addr & 0xfff)
    return -EINVAL;
  uint64_t n = PAGE_UP((uint64_t)len) >> 12;
  for (uint64_t i = 0; i < n; i++) {
    uint64_t a = (uint64_t)addr + i * PAGE_SIZE;
    if (!vma_find(mm, a))
      return -ENOMEM;
    uint64_t *pte = pte_get(mm->pml4, a, false);
    uint8_t b = pte && (*pte & PTE_P);
    if (copy_to_user((char *)uvec + i, &b, 1))
      return -EFAULT;
  }
  return 0;
}
