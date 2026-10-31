/* Static TLS set-up for the main thread, and TLS copying for new threads.
 *
 * Memory layout of a thread's TLS area (x86-64, TLS variant II):
 *
 *   mem [ dtv ][pad][ module blocks, offsets down from tp ][ struct __pthread ] ...
 *                                                         ^ thread pointer (%fs)
 *
 * The modules of the static TLS area are the executable's (id 1) and, in
 * a dynamically linked program, those of the shared objects loaded at
 * start-up (the dynamic linker lays them out).  dtv[0] is the number of
 * entries, dtv[id] the address of module id's block; dlopen'ed modules
 * get theirs on first use (__tls_get_addr).
 */
#include <stddef.h>
#include <string.h>
#include "libc.h"
#include "elf_min.h"
#include "pthread_impl.h"
#include "syscall.h"

unsigned long __getauxval(unsigned long);

#define BUILTIN_TLS_SIZE 1024
static unsigned long builtin_tls[(BUILTIN_TLS_SIZE + sizeof(struct __pthread)) / sizeof(long) + 2];
static struct __tls_module main_tls;

#define ARCH_SET_FS 0x1002

int __set_thread_area(void *p)
{
	return (int)__syscall2(SYS_arch_prctl, ARCH_SET_FS, p);
}

/* The area's size and alignment from the modules (__libc.tls_head, with
 * their offsets set). */
void __tls_layout(void)
{
	struct __tls_module *t;
	size_t maxoff = 0, n = 0;
	if (__libc.tls_align < _Alignof(struct __pthread)) __libc.tls_align = _Alignof(struct __pthread);
	for (t = __libc.tls_head; t; t = t->next) {
		if (t->offset > maxoff) maxoff = t->offset;
		if (t->align > __libc.tls_align) __libc.tls_align = t->align;
		n++;
	}
	__libc.tls_cnt = n;
	__libc.tls_area_size = (n + 1) * sizeof(unsigned long) + maxoff
	                       + sizeof(struct __pthread) + __libc.tls_align;
	__libc.tls_area_size = (__libc.tls_area_size + 15) & -(size_t)16;
}

/* Lay out a TLS area in mem (at least __libc.tls_area_size bytes) and
 * return its thread control block. */
struct __pthread *__copy_tls(unsigned char *mem)
{
	struct __tls_module *t;
	unsigned long *dtv = (unsigned long *)(void *)mem;
	uintptr_t tp = (uintptr_t)mem + (__libc.tls_cnt + 1) * sizeof(unsigned long);
	struct __pthread *td;
	size_t maxoff = 0;

	for (t = __libc.tls_head; t; t = t->next)
		if (t->offset > maxoff) maxoff = t->offset;
	tp = (tp + maxoff + __libc.tls_align - 1) & -(uintptr_t)__libc.tls_align;
	td = (struct __pthread *)tp;
	dtv[0] = __libc.tls_cnt;
	for (t = __libc.tls_head; t; t = t->next) {
		unsigned char *block = (unsigned char *)tp - t->offset;
		if (t->len) memcpy(block, t->image, t->len);
		if (t->size > t->len) memset(block + t->len, 0, t->size - t->len);
		dtv[t->id] = (unsigned long)block;
	}
	td->dtv = td->dtv_static = dtv;
	td->self = td->self2 = td;
	return td;
}

/* The main thread's descriptor, once its TLS area is set up. */
void __init_tp(struct __pthread *td)
{
	const unsigned char *rnd;
	if (__set_thread_area(td) < 0) for (;;) __syscall1(SYS_exit_group, 127);

	/* Stack-protector canary from the kernel's AT_RANDOM bytes; the low
	 * byte is zeroed so string overflows cannot reproduce it. */
	rnd = (const unsigned char *)__getauxval(AT_RANDOM);
	if (rnd) {
		memcpy(&td->canary, rnd, sizeof td->canary);
		memcpy(&td->pointer_guard, rnd + 8, sizeof td->pointer_guard);
	}
	td->canary &= ~0xffUL;
	/* the kernel clears detach_state (to DT_EXITED) when the thread ends,
	 * which is what pthread_join() waits for */
	td->tid = (int)__syscall1(SYS_set_tid_address, &td->detach_state);
	td->detach_state = DT_JOINABLE;
	td->robust_list.head = &td->robust_list.head;
	td->next = td->prev = td;
}

/* Allocate the main thread's TLS area (__libc laid out) and install it. */
void __init_main_tls(void)
{
	unsigned char *mem;
	if (__libc.tls_area_size <= sizeof builtin_tls) {
		mem = (unsigned char *)builtin_tls;
	} else {
		long r = __syscall6x(SYS_mmap, 0, __libc.tls_area_size, 3 /*RW*/,
		                     0x22 /*PRIVATE|ANON*/, -1, 0);
		if ((unsigned long)r > -4096UL) for (;;) __syscall1(SYS_exit_group, 127);
		mem = (unsigned char *)r;
	}
	__init_tp(__copy_tls(mem));
}

/* A statically linked program: the executable's PT_TLS is the one module. */
void __init_tls(unsigned long *auxv)
{
	const Elf64_Phdr *ph = (const Elf64_Phdr *)__getauxval(AT_PHDR);
	size_t phnum = __getauxval(AT_PHNUM), phent = __getauxval(AT_PHENT);
	const Elf64_Phdr *tls_ph = 0;
	uintptr_t base = 0;
	struct __tls_module *t = &main_tls;
	size_t i;
	int have_base = 0;
	(void)auxv;

	for (i = 0; ph && i < phnum; i++) {
		const Elf64_Phdr *p = (const Elf64_Phdr *)((const char *)ph + i * phent);
		if (p->p_type == PT_PHDR) base = (uintptr_t)ph - p->p_vaddr, have_base = 1;
		if (p->p_type == PT_TLS) tls_ph = p;
	}
	/* no PT_PHDR (a static PIE): the headers follow the ELF header at the
	 * start of the segment with file offset 0 */
	for (i = 0; ph && !have_base && i < phnum; i++) {
		const Elf64_Phdr *p = (const Elf64_Phdr *)((const char *)ph + i * phent);
		if (p->p_type == PT_LOAD && !p->p_offset) base = (uintptr_t)ph - 64 - p->p_vaddr, have_base = 1;
	}
	if (tls_ph) {
		t->image = (void *)(base + tls_ph->p_vaddr);
		t->len = tls_ph->p_filesz;
		t->size = tls_ph->p_memsz;
		t->align = tls_ph->p_align ? tls_ph->p_align : 1;
		t->offset = (t->size + t->align - 1) & -t->align;
		t->id = 1;
		__libc.tls_head = t;
	}
	__tls_layout();
	__init_main_tls();
}
