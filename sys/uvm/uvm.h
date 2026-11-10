/* uvm.h - the memory system's structures: pages, shared memory objects,
   VMAs, address spaces. */
#ifndef _UVM_UVM_H_
#define _UVM_UVM_H_

#include <sys/systm.h>
#include <uvm/uvm_extern.h>

/* physical pages (mm/page.c) */
struct page {
  uint32_t next, prev; /* buddy free list (page frame numbers) */
  uint16_t ref;
  uint8_t order;
  uint8_t flags;
};
#define PG_FREE 1
#define PG_SLAB 2
#define PG_LARGE 4
#define PG_RESERVED 8

extern struct page *pages;
static inline struct page *phys_page(uint64_t phys) { return &pages[phys >> 12]; }

/* shared memory object: anonymous shared mappings, SysV shm, tmpfs files,
   and the page cache of a file on a block file system (fs/pcache.c) */
struct shmobj {
  int ref;
  size_t npages;
  uint64_t *page;      /* physical addresses, 0 = not yet allocated; in a
                          page cache bit 0 (SHM_CAND) marks a page a shared
                          mapping may have changed */
  struct inode *owner; /* page cache: the file, else NULL */
  void (*release)(struct shmobj *o); /* when the last reference goes (typed memory) */
  void *priv;
};
#define SHM_CAND 1UL
#define SHM_ADDR (~0xfffUL)
struct vma {
  struct list link;
  uint64_t start, end;
  unsigned flags; /* VM_* */
  struct file *file;     /* file mapping, or NULL */
  uint64_t offset;       /* file offset of start */
  struct shmobj *shm;    /* shared memory, or NULL */
  struct shmobj *pc;     /* a private file mapping: the file's page cache,
                            whose pages it maps copy on write (or NULL) */
  uint64_t shm_off;      /* page index of start in shm */
  int shmid;             /* SysV segment, -1 if none */
  unsigned attach;       /* which shmat (pieces of one share it) */
};

struct mm {
  int ref;
  uint64_t pml4;         /* physical */
  struct list vmas;      /* sorted by address */
  uint64_t brk_start, brk;
  uint64_t mmap_base;
  uint64_t stack_top;
  uint64_t arg_start, arg_end, env_start, env_end;
  uint64_t rss;
  bool lock_future; /* mlockall(MCL_FUTURE) */
  int nvmas;         /* VMAs, for MAX_MAP_COUNT */
  uint64_t alloc_lo; /* where vma_free_area last placed a mapping */
  struct vma *alloc_hint; /* the VMA made there (NULL when gone) */
};
#define MAX_MAP_COUNT 65530 /* mappings per process (as Linux's default) */

#endif
