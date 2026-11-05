/* mm.h - physical pages, kernel heap, address spaces. */
#ifndef MM_H
#define MM_H

#include "kernel.h"

struct regs;
struct file;
struct inode;

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
extern uint64_t max_pfn, total_pages, free_pages_count;
void page_init(uint64_t kernel_end_phys, const uint64_t *ranges, int nranges,
               const uint64_t *reserved, int nreserved);
void page_init_high(const uint64_t *ranges, int nranges, const uint64_t *reserved,
                    int nreserved);
uint64_t alloc_pages(int order); /* physical address, 0 if none */
void free_pages(uint64_t phys, int order);
uint64_t alloc_page(void);       /* one page, ref 1, not cleared */
uint64_t alloc_zeroed_page(void);
void page_get(uint64_t phys);
void page_put(uint64_t phys);    /* drop a reference, free at 0 */
static inline struct page *phys_page(uint64_t phys) { return &pages[phys >> 12]; }

/* kernel heap (mm/kmalloc.c) */
void *kmalloc(size_t n);
void *kzalloc(size_t n);
void kfree(void *p);
void *krealloc(void *p, size_t old, size_t n);
char *kstrdup(const char *s);

/* page table entries */
#define PTE_P 0x1UL
#define PTE_W 0x2UL
#define PTE_U 0x4UL
#define PTE_PWT 0x8UL
#define PTE_PCD 0x10UL
#define PTE_A 0x20UL
#define PTE_D 0x40UL
#define PTE_PS 0x80UL
#define PTE_G 0x100UL
#define PTE_COW 0x200UL /* software: copy on write */
#define PTE_NX (1UL << 63)
#define PTE_ADDR 0x000ffffffffff000UL

/* mmap/mprotect (Linux values) */
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_SHARED_VALIDATE 0x03
#define MAP_TYPE 0x0f
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_GROWSDOWN 0x100
#define MAP_DENYWRITE 0x800
#define MAP_EXECUTABLE 0x1000
#define MAP_LOCKED 0x2000
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000
#define MAP_NONBLOCK 0x10000
#define MAP_STACK 0x20000
#define MAP_FIXED_NOREPLACE 0x100000

/* VMA flags */
#define VM_READ 1
#define VM_WRITE 2
#define VM_EXEC 4
#define VM_SHARED 8
#define VM_GROWSDOWN 0x100
#define VM_LOCKED 0x2000

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
struct shmobj *shmobj_new(size_t npages);
void shmobj_get(struct shmobj *o);
void shmobj_put(struct shmobj *o);
uint64_t shmobj_page(struct shmobj *o, size_t idx); /* allocates (zeroed) */
int shmobj_resize(struct shmobj *o, size_t npages);
void shmobj_unmap_from(struct shmobj *o, size_t idx); /* from every address space */

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

extern uint64_t kernel_pml4; /* physical */
void vm_init(void);
struct mm *mm_new(void);
struct mm *mm_dup(struct mm *old); /* fork: copy on write */
void mm_put(struct mm *mm);
void mm_switch(struct mm *mm);
uint64_t *pte_get(uint64_t pml4, uint64_t va, bool create);
int map_page(struct mm *mm, uint64_t va, uint64_t phys, uint64_t flags);
void unmap_range(struct mm *mm, uint64_t start, uint64_t end);
void flush_tlb(void);
uint64_t vm_prot_pte(unsigned vmflags);
void *ioremap(uint64_t phys, size_t size); /* device memory, uncached */

struct vma *vma_find(struct mm *mm, uint64_t addr);  /* the VMA containing addr */
struct vma *vma_new(struct mm *mm, uint64_t start, uint64_t end, unsigned flags);
int vma_unmap(struct mm *mm, uint64_t start, uint64_t end);
struct vma *vma_split_at(struct mm *mm, uint64_t addr);
void vma_protect_range(struct mm *mm, uint64_t start, uint64_t end, unsigned prot);
uint64_t vma_free_area(struct mm *mm, uint64_t len, uint64_t hint);
int mm_populate(struct mm *mm, uint64_t start, uint64_t end, bool write);
long do_mmap(uint64_t addr, uint64_t len, int prot, int flags, struct file *f, uint64_t off);

void page_fault(struct regs *r);
int handle_fault(struct mm *mm, uint64_t addr, bool write, bool user);
void kernel_fault(struct regs *r, uint64_t addr);

/* checked access to user memory */
bool user_range_ok(uint64_t addr, size_t n);
int copy_from_user(void *dst, const void *usrc, size_t n);
int copy_to_user(void *udst, const void *src, size_t n);
int clear_user(void *udst, size_t n);
long strncpy_from_user(char *dst, const char *usrc, size_t max);
char *user_path(const char *upath); /* kmalloc'ed copy, or ERR_PTR */
#define get_user(dst, uptr) copy_from_user(&(dst), (uptr), sizeof(dst))

#endif
