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

/* uvm_extern.h - the memory system's interfaces: physical pages, kernel heap,
   address spaces, access to user memory.  uvm.h has its structures. */
#ifndef _UVM_UVM_EXTERN_H_
#define _UVM_UVM_EXTERN_H_

#include <sys/systm.h>

struct regs;
struct file;
struct inode;
struct page;
struct vma;
struct mm;
struct shmobj;

/* physical pages (uvm/uvm_page.c) */
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

/* kernel heap (kern/kern_malloc.c) */
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

struct shmobj *shmobj_new(size_t npages);
void shmobj_get(struct shmobj *o);
void shmobj_put(struct shmobj *o);
uint64_t shmobj_page(struct shmobj *o, size_t idx); /* allocates (zeroed) */
int shmobj_resize(struct shmobj *o, size_t npages);
void shmobj_unmap_from(struct shmobj *o, size_t idx); /* from every address space */

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
