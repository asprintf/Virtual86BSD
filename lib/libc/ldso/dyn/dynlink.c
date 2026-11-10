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

/* The dynamic linker.  libc.so is its own program interpreter: a
 * dynamically linked program names it in PT_INTERP, the kernel maps it
 * and starts it at _dlstart (dlstart.s), and it
 *
 *   1. relocates itself (__dls1: only R_X86_64_RELATIVE, using nothing
 *      that needs a relocation; __dls2: its symbolic relocations against
 *      its own symbols, after which all of the C library works);
 *   2. sets up a provisional thread pointer, finds the program (AT_PHDR),
 *      loads LD_PRELOAD and the DT_NEEDED shared objects breadth-first
 *      (libc.so, libm.so, libpthread.so ... are itself), lays out the
 *      static TLS of all of them, relocates them (dependencies first,
 *      itself again against the global scope: the program may have copies
 *      of its variables), makes RELRO read-only, installs the real thread
 *      pointer and jumps to the program's entry point;
 *   3. runs the constructors (dependencies before their users, the
 *      program's last) when __libc_start_main calls __libc.dl_init, and
 *      the destructors in reverse order at exit.
 *
 * Binding is always immediate (as with LD_BIND_NOW); dlclose does not
 * unload (POSIX allows it).  dlopen'ed objects with TLS get dynamic TLS
 * (__tls_get_addr allocates a thread's block on first use); an object
 * with initial-exec TLS references cannot be dlopen'ed.  Run directly,
 * "libc.so program args..." loads and runs the program.
 *
 * Built by gcc with -Bsymbolic-functions, or by c17 -G (which binds the
 * same way): calls inside libc.so bind to libc.so, data references go
 * through the GOT, so a program's copy relocations work. */
#define _GNU_SOURCE 1
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "libc.h"
#include "pthread_impl.h"
#include "lock.h"
#include "syscall.h"

#define HIDDEN __attribute__((visibility("hidden")))
#ifndef RTLD_DEFAULT
#define RTLD_NEXT    ((void *)-1)
#define RTLD_DEFAULT ((void *)0)
#endif

/* ---- ELF ---- */

typedef struct {
	unsigned char e_ident[16];
	uint16_t e_type, e_machine;
	uint32_t e_version;
	uint64_t e_entry, e_phoff, e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Ehdr;

typedef struct {
	uint32_t p_type, p_flags;
	uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Phdr;

typedef struct { int64_t d_tag; uint64_t d_val; } Dyn;

typedef struct {
	uint32_t st_name;
	unsigned char st_info, st_other;
	uint16_t st_shndx;
	uint64_t st_value, st_size;
} Sym;

typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } Rela;

enum {
	PT_LOAD = 1, PT_DYNAMIC = 2, PT_INTERP = 3, PT_PHDR = 6, PT_TLS = 7,
	PT_GNU_RELRO = 0x6474e552
};
enum {
	DT_NULL = 0, DT_NEEDED = 1, DT_PLTRELSZ = 2, DT_HASH = 4, DT_STRTAB = 5,
	DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8, DT_INIT = 12, DT_FINI = 13,
	DT_SONAME = 14, DT_RPATH = 15, DT_DEBUG = 21, DT_TEXTREL = 22, DT_JMPREL = 23,
	DT_INIT_ARRAY = 25, DT_FINI_ARRAY = 26, DT_INIT_ARRAYSZ = 27,
	DT_FINI_ARRAYSZ = 28, DT_RUNPATH = 29, DT_FLAGS = 30,
	DT_PREINIT_ARRAY = 32, DT_PREINIT_ARRAYSZ = 33, DT_RELRSZ = 35, DT_RELR = 36,
	DT_NUM = 37
};
#define DT_GNU_HASH 0x6ffffef5
#define DT_VERSYM   0x6ffffff0
#define DF_TEXTREL  4

enum {
	R_NONE = 0, R_64 = 1, R_PC32 = 2, R_COPY = 5, R_GLOB_DAT = 6, R_JUMP_SLOT = 7,
	R_RELATIVE = 8, R_DTPMOD64 = 16, R_DTPOFF64 = 17, R_TPOFF64 = 18, R_IRELATIVE = 37
};

#define STB_LOCAL 0
#define STB_WEAK 2
#define STB_GNU_UNIQUE 10
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_COMMON 5
#define STT_TLS 6
#define STT_GNU_IFUNC 10
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_SECURE 23

/* ---- the loaded objects ---- */

struct dso {
	/* struct link_map, as debuggers read it (r_debug.r_map) */
	unsigned char *base;
	char *name;
	Dyn *dynv;
	struct dso *next, *prev;

	Phdr *phdr;
	size_t phnum, phentsize;
	Sym *syms;
	uint32_t *hashtab, *ghashtab;
	uint16_t *versym;
	char *strings;
	size_t dyn[DT_NUM];        /* the values of the tags below DT_NUM */
	unsigned char *map;
	size_t map_len;
	dev_t dev;
	ino_t ino;
	char *shortname;
	char *rpath;               /* DT_RUNPATH, or DT_RPATH */
	int runpath;               /* rpath is a DT_RUNPATH */
	int relocated, constructed, global, nodelete;
	int refcnt;
	struct dso **deps;         /* the direct dependencies */
	size_t ndeps;
	struct dso **scope;        /* itself and its dependencies, breadth-first (dlsym, a local object's lookups) */
	size_t nscope;
	struct __tls_module tls;   /* tls.size 0: none */
	int static_tls;            /* in the static TLS area */
	size_t relro_start, relro_end;
	struct dso *fini_next;
};

/* r_debug, for debuggers (the program's DT_DEBUG points to it) */
struct r_debug {
	int r_version;
	struct dso *r_map;
	void (*r_brk)(void);
	int r_state;
	void *r_ldbase;
};
struct r_debug _r_debug;
void _dl_debug_state(void) __attribute__((noinline));
void _dl_debug_state(void) { __asm__ __volatile__(""); }

static struct dso ldso, app, *head, *tail, *fini_head;
static char *env_path, *env_preload;
static char ldso_dir[PATH_MAX];
static size_t page = 4096;
static int runtime;            /* the program runs: errors are dlopen's, not fatal */
static size_t tls_static_offset, tls_dyn_cnt;
static struct __tls_module *tls_tail;
static struct dso **init_list;
static size_t ninit, init_cap;
static volatile int lk;
static volatile int lk_owner;
static int lk_count;

extern const Ehdr __ehdr_start HIDDEN;
extern Dyn _DYNAMIC[] HIDDEN;
HIDDEN _Noreturn void __dls_jump(size_t entry, size_t *sp);
void *__selfsym_lookup(const char *);
int __selfsym_addr(const void *, Dl_info_t *);
extern const char *__selfsym_file;
void *__tls_get_addr(unsigned long *);

static const char *const builtin_names[] = {
	"libc.so", "libm.so", "libpthread.so", "librt.so", "libdl.so", "libxnet.so",
	"libcrypt.so", "libutil.so", 0
};

/* ---- errors ---- */

static char errbuf_startup[256];

static void error(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	struct __pthread *self = __pthread_self();
	if (!runtime) {
		va_start(ap, fmt);
		vsnprintf(errbuf_startup, sizeof errbuf_startup, fmt, ap);
		va_end(ap);
		dprintf(2, "%s: %s\n", ldso.name ? ldso.name : "libc.so", errbuf_startup);
		_exit(127);
	}
	/* a message per thread, kept until the next error of that thread */
	buf = malloc(256);
	if (!buf) {
		self->dlerror = "dynamic linker: out of memory";
		return;
	}
	va_start(ap, fmt);
	vsnprintf(buf, 256, fmt, ap);
	va_end(ap);
	free((void *)(uintptr_t)self->dlerror_buf);
	self->dlerror_buf = buf;
	self->dlerror = buf;
}

/* ---- dynamic sections and symbols ---- */

static void decode_vec(Dyn *v, size_t *a, size_t cnt)
{
	size_t i;
	for (i = 0; i < cnt; i++) a[i] = 0;
	for (; v->d_tag; v++)
		if ((uint64_t)v->d_tag < cnt) {
			a[0] |= 1UL << v->d_tag;
			a[v->d_tag] = v->d_val;
		}
}

static size_t dyn_lookup(Dyn *v, int64_t tag)
{
	for (; v->d_tag; v++)
		if (v->d_tag == tag) return v->d_val;
	return 0;
}

static void decode_dyn(struct dso *p)
{
	size_t *a = p->dyn;
	decode_vec(p->dynv, a, DT_NUM);
	p->syms = (Sym *)(p->base + a[DT_SYMTAB]);
	p->strings = (char *)(p->base + a[DT_STRTAB]);
	if (a[0] & (1UL << DT_HASH)) p->hashtab = (uint32_t *)(p->base + a[DT_HASH]);
	if (a[0] & (1UL << DT_RUNPATH)) { p->rpath = p->strings + a[DT_RUNPATH]; p->runpath = 1; }
	else if (a[0] & (1UL << DT_RPATH)) p->rpath = p->strings + a[DT_RPATH];
	{
		size_t g = dyn_lookup(p->dynv, DT_GNU_HASH), vs = dyn_lookup(p->dynv, DT_VERSYM);
		if (g) p->ghashtab = (uint32_t *)(p->base + g);
		if (vs) p->versym = (uint16_t *)(p->base + vs);
	}
}

static uint32_t sysv_hash(const char *s0)
{
	const unsigned char *s = (const void *)s0;
	uint32_t h = 0;
	while (*s) {
		h = 16 * h + *s++;
		h ^= h >> 24 & 0xf0;
	}
	return h & 0xfffffff;
}

static uint32_t gnu_hash(const char *s0)
{
	const unsigned char *s = (const void *)s0;
	uint32_t h = 5381;
	for (; *s; s++) h += h * 32 + *s;
	return h;
}

#define OK_TYPES (1 << STT_NOTYPE | 1 << STT_OBJECT | 1 << STT_FUNC | 1 << STT_COMMON | 1 << STT_TLS | 1 << STT_GNU_IFUNC)
#define OK_BINDS (1 << 1 | 1 << STB_WEAK | 1 << STB_GNU_UNIQUE)

/* plt: for a PLT slot, which a program's canonical PLT entry (an
 * undefined function with an address: the function's address in a
 * program that is not position-independent) must not satisfy */
static int plt_lookup;

static int sym_ok(struct dso *p, size_t i)
{
	Sym *s = &p->syms[i];
	if (!s->st_shndx && (plt_lookup || !s->st_value || (s->st_info & 15) != STT_FUNC)) return 0;
	if (!(1 << (s->st_info & 15) & OK_TYPES)) return 0;
	if (!(1 << (s->st_info >> 4) & OK_BINDS)) return 0;
	if (p->versym && (p->versym[i] & 0x8000)) return 0;   /* a hidden (non-default) version */
	return 1;
}

static Sym *lookup_sysv(struct dso *p, const char *name, uint32_t h)
{
	uint32_t *ht = p->hashtab, i;
	for (i = ht[2 + h % ht[0]]; i; i = ht[2 + ht[0] + i])
		if (!strcmp(name, p->strings + p->syms[i].st_name) && sym_ok(p, i))
			return &p->syms[i];
	return 0;
}

static Sym *lookup_gnu(struct dso *p, const char *name, uint32_t h)
{
	uint32_t *g = p->ghashtab, nb = g[0], off = g[1], bloom = g[2], shift = g[3], i, h2;
	const uint64_t *bl = (const uint64_t *)(void *)(g + 4);
	const uint32_t *buckets = (const uint32_t *)(bl + bloom), *chain;
	uint64_t w = bl[(h / 64) % bloom];
	if (!(w >> (h % 64) & w >> ((h >> shift) % 64) & 1)) return 0;
	if (!(i = buckets[h % nb])) return 0;
	chain = buckets + nb - off;
	for (;; i++) {
		h2 = chain[i];
		if ((h | 1) == (h2 | 1) && !strcmp(name, p->strings + p->syms[i].st_name) && sym_ok(p, i))
			return &p->syms[i];
		if (h2 & 1) return 0;
	}
}

static Sym *lookup(struct dso *p, const char *name, uint32_t gh, uint32_t sh)
{
	if (p->ghashtab) return lookup_gnu(p, name, gh);
	if (p->hashtab) return lookup_sysv(p, name, sh);
	return 0;
}

/* the number of dynamic symbols */
static size_t count_syms(struct dso *p)
{
	if (p->hashtab) return p->hashtab[1];
	if (p->ghashtab) {
		uint32_t *g = p->ghashtab, nb = g[0], off = g[1], bloom = g[2], i, max = 0;
		const uint32_t *buckets = (const uint32_t *)(void *)((const uint64_t *)(void *)(g + 4) + bloom);
		const uint32_t *chain = buckets + nb - off;
		for (i = 0; i < nb; i++) if (buckets[i] > max) max = buckets[i];
		if (!max) return off;
		while (!(chain[max] & 1)) max++;
		return max + 1;
	}
	return 0;
}

struct def { Sym *sym; struct dso *dso; };

/* the first definition of name in the global scope, then in the
 * objects of extra (a dlopen'ed local object's own scope); skip: an
 * object not to look in (the program, for a copy relocation) */
static struct def find_sym(const char *name, struct dso **extra, size_t nextra, struct dso *skip)
{
	uint32_t gh = gnu_hash(name), sh = sysv_hash(name);
	struct def d = { 0, 0 };
	struct dso *p;
	size_t i;
	for (p = head; p; p = p->next) {
		if (!p->global || p == skip) continue;
		if ((d.sym = lookup(p, name, gh, sh))) { d.dso = p; return d; }
	}
	for (i = 0; i < nextra; i++) {
		p = extra[i];
		if (p->global || p == skip) continue;
		if ((d.sym = lookup(p, name, gh, sh))) { d.dso = p; return d; }
	}
	d.sym = 0;
	return d;
}

/* ---- relocation ---- */

static size_t sym_addr(struct def d)
{
	size_t a = (size_t)d.dso->base + d.sym->st_value;
	if ((d.sym->st_info & 15) == STT_GNU_IFUNC) a = ((size_t (*)(void))a)();
	return a;
}

static void do_relocs(struct dso *p, Rela *rel, size_t size, int skip_relative)
{
	unsigned char *base = p->base;
	Rela *end = (Rela *)(void *)((char *)rel + size);
	for (; rel < end; rel++) {
		size_t type = rel->r_info & 0xffffffff, symi = rel->r_info >> 32;
		size_t *reloc_addr = (size_t *)(void *)(base + rel->r_offset);
		Sym *sym = symi ? &p->syms[symi] : 0;
		const char *name = sym ? p->strings + sym->st_name : 0;
		struct def def = { 0, 0 };
		size_t s = 0;

		if (type == R_NONE) continue;
		if (type == R_RELATIVE) {
			if (!skip_relative) *reloc_addr = (size_t)base + rel->r_addend;
			continue;
		}
		if (sym && (sym->st_info >> 4) == STB_LOCAL) {
			def.sym = sym;
			def.dso = p;
		} else if (sym) {
			plt_lookup = type == R_JUMP_SLOT;
			def = find_sym(name, p->scope, p->nscope, type == R_COPY ? p : 0);
			plt_lookup = 0;
			if (!def.sym) {
				if ((sym->st_info >> 4) != STB_WEAK) {
					error("Error relocating %s: %s: symbol not found", p->name, name);
					if (runtime) { p->relocated = -1; return; }
				}
				def.sym = 0;
			}
		} else {
			def.dso = p;
		}
		if (def.sym && (def.sym->st_info & 15) != STT_TLS) s = sym_addr(def);

		switch (type) {
		case R_64:
			*reloc_addr = s + rel->r_addend;
			break;
		case R_GLOB_DAT:
		case R_JUMP_SLOT:
			*reloc_addr = s + rel->r_addend;
			break;
		case R_PC32:
			*(uint32_t *)(void *)reloc_addr = (uint32_t)(s + rel->r_addend - (size_t)reloc_addr);
			break;
		case R_COPY:
			if (def.sym) memcpy(reloc_addr, (void *)s, sym->st_size);
			break;
		case R_DTPMOD64:
			*reloc_addr = def.dso ? def.dso->tls.id : 0;
			break;
		case R_DTPOFF64:
			*reloc_addr = (def.sym ? def.sym->st_value : 0) + rel->r_addend;
			break;
		case R_TPOFF64:
			if (def.dso && !def.dso->static_tls) {
				error("Error relocating %s: %s: initial-exec TLS of a dynamically loaded object",
				      p->name, name ? name : "(local)");
				if (runtime) { p->relocated = -1; return; }
			}
			*reloc_addr = (def.sym ? def.sym->st_value : 0) + rel->r_addend
			              - (def.dso ? def.dso->tls.offset : 0);
			break;
		case R_IRELATIVE:
			*reloc_addr = ((size_t (*)(void))(base + rel->r_addend))();
			break;
		default:
			error("Error relocating %s: unsupported relocation type %d", p->name, (int)type);
			if (runtime) { p->relocated = -1; return; }
		}
	}
}

static void relr(unsigned char *base, size_t *r, size_t size)
{
	size_t *end = (size_t *)(void *)((char *)r + size), *where = 0;
	for (; r < end; r++) {
		if (!(*r & 1)) {
			where = (size_t *)(void *)(base + *r);
			*where++ += (size_t)base;
		} else {
			size_t bits = *r >> 1, i;
			for (i = 0; bits; bits >>= 1, i++)
				if (bits & 1) where[i] += (size_t)base;
			where += 63;
		}
	}
}

static int relocate(struct dso *p, int skip_relative)
{
	size_t *a = p->dyn;
	if (p->dyn[DT_RELR] && !skip_relative) relr(p->base, (size_t *)(void *)(p->base + a[DT_RELR]), a[DT_RELRSZ]);
	if (a[DT_RELA]) do_relocs(p, (Rela *)(void *)(p->base + a[DT_RELA]), a[DT_RELASZ], skip_relative);
	if (p->relocated < 0) return -1;
	if (a[DT_JMPREL]) do_relocs(p, (Rela *)(void *)(p->base + a[DT_JMPREL]), a[DT_PLTRELSZ], skip_relative);
	if (p->relocated < 0) return -1;
	p->relocated = 1;
	if (p->relro_end > p->relro_start)
		mprotect(p->base + p->relro_start, p->relro_end - p->relro_start, PROT_READ);
	return 0;
}

/* ---- stage 1 and 2: relocating libc.so itself ---- */

HIDDEN void __dls2(unsigned char *base, size_t *sp);

/* Only the relative relocations, with no other help: nothing here may
 * need a relocation (no global data, no calls but to __dls2). */
HIDDEN void __dls1(size_t *sp)
{
	unsigned char *base = (unsigned char *)&__ehdr_start;
	Dyn *v;
	Rela *rel = 0;
	size_t relsz = 0, *rr = 0, rrsz = 0, i;
	for (v = _DYNAMIC; v->d_tag; v++) {
		if (v->d_tag == DT_RELA) rel = (Rela *)(void *)(base + v->d_val);
		else if (v->d_tag == DT_RELASZ) relsz = v->d_val;
		else if (v->d_tag == DT_RELR) rr = (size_t *)(void *)(base + v->d_val);
		else if (v->d_tag == DT_RELRSZ) rrsz = v->d_val;
	}
	for (i = 0; rel && i < relsz / sizeof *rel; i++)
		if ((rel[i].r_info & 0xffffffff) == R_RELATIVE)
			*(size_t *)(void *)(base + rel[i].r_offset) = (size_t)base + rel[i].r_addend;
	if (rr) {
		size_t *end = (size_t *)(void *)((char *)rr + rrsz), *where = 0;
		for (; rr < end; rr++) {
			if (!(*rr & 1)) {
				where = (size_t *)(void *)(base + *rr);
				*where++ += (size_t)base;
			} else {
				size_t bits = *rr >> 1;
				for (i = 0; bits; bits >>= 1, i++)
					if (bits & 1) where[i] += (size_t)base;
				where += 63;
			}
		}
	}
	__dls2(base, sp);
}

static void dls3(size_t *sp) __attribute__((noinline));

/* Static data works now; the symbolic relocations against itself. */
HIDDEN void __dls2(unsigned char *base, size_t *sp)
{
	ldso.base = base;
	ldso.dynv = _DYNAMIC;
	ldso.name = ldso.shortname = "libc.so";
	ldso.global = 1;
	{
		const Ehdr *eh = (const Ehdr *)(void *)base;
		ldso.phdr = (Phdr *)(void *)(base + eh->e_phoff);
		ldso.phnum = eh->e_phnum;
		ldso.phentsize = eh->e_phentsize;
	}
	decode_dyn(&ldso);
	head = tail = &ldso;
	/* relocations against its own symbols only (head is itself) */
	relocate(&ldso, 1);
	ldso.relocated = 0;
	dls3(sp);
}

/* ---- loading ---- */

static void unmap_dso(struct dso *p)
{
	if (p->map) munmap(p->map, p->map_len);
}

/* Maps the object on fd into p; 0, or -1 with an error set */
static int map_library(int fd, struct dso *p)
{
	union { Ehdr eh; unsigned char b[1024]; } u;
	Phdr *ph, *ph0;
	size_t i, addr_min = SIZE_MAX, addr_max = 0, off_start, dyn = 0, tls_i = (size_t)-1;
	ssize_t l;
	unsigned char *map, *base;
	Ehdr *eh = &u.eh;

	l = pread(fd, u.b, sizeof u.b, 0);
	if (l < (ssize_t)sizeof *eh || memcmp(eh->e_ident, "\177ELF\2\1", 6)) goto noexec;
	if ((eh->e_type != 3 /* ET_DYN */ && (eh->e_type != 2 || p != &app)) || eh->e_machine != 62 /* EM_X86_64 */)
		goto noexec;
	if (eh->e_phentsize != sizeof(Phdr) || eh->e_phoff + (size_t)eh->e_phnum * sizeof(Phdr) > (size_t)l)
		goto noexec;
	ph0 = (Phdr *)(void *)(u.b + eh->e_phoff);
	for (i = 0, ph = ph0; i < eh->e_phnum; i++, ph++) {
		if (ph->p_type == PT_DYNAMIC) dyn = ph->p_vaddr;
		else if (ph->p_type == PT_TLS) tls_i = i;
		else if (ph->p_type == PT_GNU_RELRO) {
			p->relro_start = ph->p_vaddr & -page;
			p->relro_end = (ph->p_vaddr + ph->p_memsz) & -page;
		}
		if (ph->p_type != PT_LOAD) continue;
		if (ph->p_vaddr < addr_min) addr_min = ph->p_vaddr;
		if (ph->p_vaddr + ph->p_memsz > addr_max) addr_max = ph->p_vaddr + ph->p_memsz;
	}
	if (!dyn || addr_min == SIZE_MAX) goto noexec;
	addr_min &= -page;
	addr_max = (addr_max + page - 1) & -page;
	/* (a program run by libc.so that is not position-independent: at its addresses) */
	map = mmap(eh->e_type == 2 ? (void *)addr_min : 0, addr_max - addr_min, PROT_NONE,
	           MAP_PRIVATE | MAP_ANONYMOUS | (eh->e_type == 2 ? MAP_FIXED : 0), -1, 0);
	if (map == MAP_FAILED) { error("cannot map %s: %s", p->name, strerror(errno)); return -1; }
	base = eh->e_type == 2 ? 0 : map - addr_min;
	p->map = map;
	p->map_len = addr_max - addr_min;
	p->base = base;
	for (i = 0, ph = ph0; i < eh->e_phnum; i++, ph++) {
		size_t this_min, this_max, fend;
		int prot;
		if (ph->p_type != PT_LOAD) continue;
		prot = (ph->p_flags & 4 ? PROT_READ : 0) | (ph->p_flags & 2 ? PROT_WRITE : 0)
		       | (ph->p_flags & 1 ? PROT_EXEC : 0);
		this_min = ph->p_vaddr & -page;
		this_max = (ph->p_vaddr + ph->p_memsz + page - 1) & -page;
		off_start = ph->p_offset & -page;
		fend = ph->p_vaddr + ph->p_filesz;
		if (ph->p_filesz && mmap(base + this_min, ((fend + page - 1) & -page) - this_min, prot,
		                         MAP_PRIVATE | MAP_FIXED, fd, off_start) == MAP_FAILED)
			goto mapfail;
		if (ph->p_memsz > ph->p_filesz) {
			size_t zstart = fend, zpage = (fend + page - 1) & -page;
			if (zpage > zstart && ph->p_filesz) {
				if (!(prot & PROT_WRITE)) goto noexec;
				memset(base + zstart, 0, zpage - zstart);
			}
			if (!ph->p_filesz) zpage = this_min;
			if (this_max > zpage && mmap(base + zpage, this_max - zpage, prot,
			                             MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
				goto mapfail;
		}
		if (eh->e_phoff >= ph->p_offset && eh->e_phoff + eh->e_phnum * sizeof(Phdr) <= ph->p_offset + ph->p_filesz)
			p->phdr = (Phdr *)(void *)(base + ph->p_vaddr + (eh->e_phoff - ph->p_offset));
	}
	p->phnum = eh->e_phnum;
	p->phentsize = eh->e_phentsize;
	p->dynv = (Dyn *)(void *)(base + dyn);
	if (tls_i != (size_t)-1) {
		ph = ph0 + tls_i;
		p->tls.image = base + ph->p_vaddr;
		p->tls.len = ph->p_filesz;
		p->tls.size = ph->p_memsz;
		p->tls.align = ph->p_align ? ph->p_align : 1;
	}
	decode_dyn(p);
	if ((p->dyn[0] & (1UL << DT_TEXTREL)) || (dyn_lookup(p->dynv, DT_FLAGS) & DF_TEXTREL)) {
		error("%s: text relocations are not supported", p->name);
		unmap_dso(p);
		return -1;
	}
	return 0;
mapfail:
	error("cannot map %s: %s", p->name, strerror(errno));
	munmap(map, addr_max - addr_min);
	p->map = 0;
	return -1;
noexec:
	if (p->map) { munmap(p->map, p->map_len); p->map = 0; }
	error("%s: not an x86-64 shared object", p->name);
	return -1;
}

/* the directory of the program, for $ORIGIN */
static void origin_of(struct dso *p, char *buf, size_t n)
{
	const char *s = p->name, *slash;
	if (p == &app) {
		ssize_t l = readlink("/proc/self/exe", buf, n - 1);
		if (l > 0) {
			buf[l] = 0;
			if ((slash = strrchr(buf, '/'))) { buf[slash - buf] = 0; return; }
		}
	}
	slash = strrchr(s, '/');
	if (!slash) { strcpy(buf, "."); return; }
	if ((size_t)(slash - s) >= n) slash = s + n - 1;
	memcpy(buf, s, (size_t)(slash - s));
	buf[slash - s] = 0;
	if (!buf[0]) strcpy(buf, "/");
}

/* tries the directories of a colon-separated list, each with $ORIGIN
 * replaced; the fd, or -1 */
static int path_open(const char *name, const char *list, struct dso *origin, char *found, size_t n)
{
	const char *s = list, *e;
	char dir[PATH_MAX], org[PATH_MAX];
	int fd;
	if (!list) return -1;
	for (; *s; s = *e ? e + 1 : e) {
		size_t l, k = 0;
		e = strchr(s, ':');
		if (!e) e = s + strlen(s);
		l = (size_t)(e - s);
		if (!l) continue;
		/* $ORIGIN and ${ORIGIN} */
		{
			size_t i = 0;
			while (i < l && k < sizeof dir - 1) {
				size_t m = 0;
				if (s[i] == '$' && !strncmp(s + i + 1, "ORIGIN", 6)) m = 7;
				else if (s[i] == '$' && !strncmp(s + i + 1, "{ORIGIN}", 8)) m = 9;
				if (m && i + m <= l) {
					if (__libc.secure || !origin) { k = sizeof dir; break; }
					origin_of(origin, org, sizeof org);
					if (k + strlen(org) >= sizeof dir) { k = sizeof dir; break; }
					strcpy(dir + k, org);
					k += strlen(org);
					i += m;
				} else
					dir[k++] = s[i++];
			}
			if (k >= sizeof dir - 1) continue;
			dir[k] = 0;
		}
		if (snprintf(found, n, "%s/%s", dir, name) >= (int)n) continue;
		if ((fd = open(found, O_RDONLY | O_CLOEXEC)) >= 0) return fd;
		if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) return -1;
	}
	return -1;
}

/* Loads name (needed by needed_by: its run path and $ORIGIN); returns
 * the object, possibly one already loaded, or 0 with an error set */
static struct dso *load_library(const char *name, struct dso *needed_by)
{
	char buf[PATH_MAX];
	const char *shortname;
	struct dso *p, tmp;
	struct stat st;
	int fd = -1, i;

	if (!*name) { error("empty library name"); return 0; }
	shortname = strrchr(name, '/') ? strrchr(name, '/') + 1 : name;
	/* the C library's own names */
	if (!strchr(name, '/'))
		for (i = 0; builtin_names[i]; i++)
			if (!strcmp(name, builtin_names[i])) return &ldso;
	/* loaded already, by name */
	for (p = head; p; p = p->next)
		if (p->shortname && !strcmp(p->shortname, shortname) && (!strchr(name, '/') || !strcmp(p->name, name)))
			return p;
	if (strchr(name, '/')) {
		if (strlen(name) >= sizeof buf) { error("%s: name too long", name); return 0; }
		strcpy(buf, name);
		fd = open(name, O_RDONLY | O_CLOEXEC);
	} else {
		struct dso *q;
		if (strlen(name) > NAME_MAX) { error("%s: name too long", name); return 0; }
		fd = path_open(name, env_path, 0, buf, sizeof buf);
		/* DT_RPATH of the requester and those that loaded it (if it has no
		 * DT_RUNPATH), else its DT_RUNPATH */
		if (fd < 0 && needed_by && needed_by->rpath && needed_by->runpath)
			fd = path_open(name, needed_by->rpath, needed_by, buf, sizeof buf);
		if (fd < 0 && needed_by && !needed_by->runpath)
			for (q = needed_by; q && fd < 0; q = q == &app ? 0 : &app)
				if (q->rpath && !q->runpath) fd = path_open(name, q->rpath, q, buf, sizeof buf);
		if (fd < 0) fd = path_open(name, ldso_dir, 0, buf, sizeof buf);
		if (fd < 0) fd = path_open(name, "/usr/local/lib:/lib:/usr/lib", 0, buf, sizeof buf);
	}
	if (fd < 0) {
		error("Error loading shared library %s: %s (needed by %s)", name,
		      errno && errno != ENOENT ? strerror(errno) : "No such file or directory",
		      needed_by ? needed_by->name : "dlopen");
		return 0;
	}
	if (fstat(fd, &st) < 0) { close(fd); error("%s: %s", buf, strerror(errno)); return 0; }
	for (p = head; p; p = p->next)
		if (p->dev == st.st_dev && p->ino == st.st_ino) { close(fd); return p; }
	if (st.st_dev == ldso.dev && st.st_ino == ldso.ino) { close(fd); return &ldso; }
	memset(&tmp, 0, sizeof tmp);
	tmp.name = buf;
	if (map_library(fd, &tmp) < 0) { close(fd); return 0; }
	close(fd);
	p = calloc(1, sizeof *p + strlen(buf) + 1);
	if (!p) { unmap_dso(&tmp); error("out of memory"); return 0; }
	*p = tmp;
	p->name = (char *)(p + 1);
	strcpy(p->name, buf);
	p->shortname = strrchr(p->name, '/') ? strrchr(p->name, '/') + 1 : p->name;
	p->dev = st.st_dev;
	p->ino = st.st_ino;
	if (p->tls.size) p->tls.id = ++tls_dyn_cnt;   /* static or dynamic decided later */
	p->prev = tail;
	tail->next = p;
	tail = p;
	return p;
}

/* the direct dependencies of p (loading them) */
static int load_direct_deps(struct dso *p)
{
	size_t n = 0, i;
	Dyn *v;
	if (p->deps || p == &ldso) return 0;
	for (v = p->dynv; v->d_tag; v++) if (v->d_tag == DT_NEEDED) n++;
	p->deps = calloc(n + 1, sizeof *p->deps);
	if (!p->deps) { error("out of memory"); return -1; }
	for (v = p->dynv, i = 0; v->d_tag; v++) {
		struct dso *q;
		if (v->d_tag != DT_NEEDED) continue;
		if (!(q = load_library(p->strings + v->d_val, p))) return -1;
		p->deps[i++] = q;
	}
	p->ndeps = i;
	return 0;
}

/* p's dependencies, breadth-first, from p on */
static int load_deps(struct dso *p)
{
	struct dso *q;
	for (q = p; q; q = q->next)
		if (load_direct_deps(q) < 0) return -1;
	return 0;
}

/* p's scope: itself and its dependencies breadth-first */
static int make_scope(struct dso *p)
{
	size_t n = 1, cap = 8, i, j, k;
	struct dso **s;
	if (p->scope) return 0;
	s = malloc(cap * sizeof *s);
	if (!s) { error("out of memory"); return -1; }
	s[0] = p;
	for (i = 0; i < n; i++)
		for (j = 0; j < s[i]->ndeps; j++) {
			struct dso *d = s[i]->deps[j];
			for (k = 0; k < n && s[k] != d; k++);
			if (k < n) continue;
			if (n == cap) {
				struct dso **t = realloc(s, (cap *= 2) * sizeof *s);
				if (!t) { free(s); error("out of memory"); return -1; }
				s = t;
			}
			s[n++] = d;
		}
	p->scope = s;
	p->nscope = n;
	return 0;
}

/* the constructor order: dependencies before the objects that need them */
static int queue_init(struct dso *p, int depth)
{
	size_t i;
	if (p->constructed || depth > 1000) return 0;
	for (i = 0; i < ninit; i++) if (init_list[i] == p) return 0;
	p->constructed = 2;     /* (being queued: dependency cycles) */
	for (i = 0; i < p->ndeps; i++)
		if (p->deps[i]->constructed != 2 && queue_init(p->deps[i], depth + 1) < 0) return -1;
	p->constructed = 0;
	if (ninit == init_cap) {
		struct dso **t = realloc(init_list, (init_cap = init_cap ? 2 * init_cap : 16) * sizeof *t);
		if (!t) { error("out of memory"); return -1; }
		init_list = t;
	}
	init_list[ninit++] = p;
	return 0;
}

static void run_init(int argc, char **argv, char **envp)
{
	while (ninit) {
		struct dso *p = init_list[0];
		size_t *a = p->dyn, i, n;
		memmove(init_list, init_list + 1, --ninit * sizeof *init_list);
		if (p->constructed) continue;
		p->constructed = 1;
		p->fini_next = fini_head;
		fini_head = p;
		if (p == &app && (a[0] & (1UL << DT_PREINIT_ARRAY))) {
			void (**f)(int, char **, char **) = (void *)(p->base + a[DT_PREINIT_ARRAY]);
			for (i = 0, n = a[DT_PREINIT_ARRAYSZ] / sizeof *f; i < n; i++) f[i](argc, argv, envp);
		}
		if (a[0] & (1UL << DT_INIT)) ((void (*)(void))(p->base + a[DT_INIT]))();
		if (a[0] & (1UL << DT_INIT_ARRAY)) {
			void (**f)(int, char **, char **) = (void *)(p->base + a[DT_INIT_ARRAY]);
			for (i = 0, n = a[DT_INIT_ARRAYSZ] / sizeof *f; i < n; i++) f[i](argc, argv, envp);
		}
	}
}

static int saved_argc;
static char **saved_argv, **saved_envp;

static void dl_init(int argc, char **argv, char **envp)
{
	saved_argc = argc;
	saved_argv = argv;
	saved_envp = envp;
	run_init(argc, argv, envp);
}

static void dl_fini(void)
{
	struct dso *p;
	for (p = fini_head; p; p = p->fini_next) {
		size_t *a = p->dyn;
		if (a[0] & (1UL << DT_FINI_ARRAY)) {
			void (**f)(void) = (void *)(p->base + a[DT_FINI_ARRAY]);
			size_t n = a[DT_FINI_ARRAYSZ] / sizeof *f;
			while (n) f[--n]();
		}
		if (a[0] & (1UL << DT_FINI)) ((void (*)(void))(p->base + a[DT_FINI]))();
	}
}

/* ---- TLS ---- */

static void add_static_tls(struct dso *p)
{
	struct __tls_module *t = &p->tls;
	if (p == &app) tls_static_offset = (t->size + t->align - 1) & -t->align;
	else tls_static_offset = (tls_static_offset + t->size + t->align - 1) & -t->align;
	t->offset = tls_static_offset;
	p->static_tls = 1;
	t->next = 0;
	if (tls_tail) tls_tail->next = t;
	else __libc.tls_head = t;
	tls_tail = t;
}

/* the TLS module with id */
static struct __tls_module *tls_module(size_t id)
{
	struct dso *p;
	for (p = head; p; p = p->next)
		if (p->tls.size && p->tls.id == id) return &p->tls;
	return 0;
}

static void *tls_get_new(unsigned long *v)
{
	struct __pthread *self = __pthread_self();
	unsigned long *dtv = self->dtv;
	struct __tls_module *t;
	unsigned char *mem;
	size_t n;
	sigset_t set, old;

	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, &old);
	LOCK(&lk);
	n = tls_dyn_cnt;
	t = tls_module(v[0]);
	UNLOCK(&lk);
	if (!t) abort();
	if (v[0] > dtv[0]) {
		unsigned long *nd = calloc(n + 1, sizeof *nd);
		if (!nd) abort();
		memcpy(nd, dtv, (dtv[0] + 1) * sizeof *nd);
		nd[0] = n;
		if (dtv != self->dtv_static) free(dtv);
		self->dtv = dtv = nd;
	}
	if (!dtv[v[0]]) {
		mem = aligned_alloc(t->align < sizeof(void *) ? sizeof(void *) : t->align,
		                    (t->size + t->align - 1) & -t->align);
		if (!mem) abort();
		memcpy(mem, t->image, t->len);
		memset(mem + t->len, 0, t->size - t->len);
		dtv[v[0]] = (unsigned long)mem;
	}
	pthread_sigmask(SIG_SETMASK, &old, 0);
	return (char *)dtv[v[0]] + v[1];
}

static void tls_free(struct __pthread *self)
{
	unsigned long *dtv = self->dtv, i;
	free(self->dlerror_buf);
	self->dlerror_buf = 0;
	if (dtv == self->dtv_static) return;
	for (i = __libc.tls_cnt + 1; i <= dtv[0]; i++) free((void *)dtv[i]);
	free(dtv);
	self->dtv = self->dtv_static;
}

/* ---- stage 3: start-up ---- */

static struct __pthread early_td;
static unsigned long early_dtv[1];

static void load_preload(char *s)
{
	char *z, c;
	for (z = s; *z; s = z) {
		for (; *s && (*s == ' ' || *s == ':'); s++);
		for (z = s; *z && *z != ' ' && *z != ':'; z++);
		c = *z;
		*z = 0;
		if (*s) {
			struct dso *p = load_library(s, 0);
			if (p) p->global = 1;
		}
		*z = c;
	}
}

static void dls3(size_t *sp)
{
	int argc = (int)sp[0];
	char **argv = (char **)(sp + 1), **envp = argv + argc + 1, **e;
	size_t *auxv, aux[32] = { 0 }, i;
	struct dso *p;
	size_t entry;

	for (e = envp; *e; e++);
	auxv = (size_t *)(e + 1);
	for (i = 0; auxv[i]; i += 2)
		if (auxv[i] < 32) aux[auxv[i]] = auxv[i + 1];
	__libc.auxv = (unsigned long *)auxv;
	__libc.page_size = page = aux[AT_PAGESZ] ? aux[AT_PAGESZ] : 4096;
	__libc.secure = aux[AT_SECURE] != 0;

	/* a provisional thread pointer: errno, locks, malloc */
	early_td.self = early_td.self2 = &early_td;
	early_td.dtv = early_td.dtv_static = early_dtv;
	early_td.tid = (int)__syscall0(SYS_gettid);
	early_td.robust_list.head = &early_td.robust_list.head;
	early_td.next = early_td.prev = &early_td;
	__set_thread_area(&early_td);

	for (e = envp; *e; e++) {
		if (!__libc.secure && !strncmp(*e, "LD_LIBRARY_PATH=", 16)) env_path = *e + 16;
		else if (!__libc.secure && !strncmp(*e, "LD_PRELOAD=", 11)) env_preload = *e + 11;
	}

	/* where libc.so is: its directory is searched after LD_LIBRARY_PATH */
	{
		struct stat st;
		const char *interp = 0;
		Phdr *ph = (Phdr *)aux[AT_PHDR];
		if (aux[AT_BASE]) {
			int have = 0;
			for (i = 0; i < aux[AT_PHNUM]; i++, ph = (Phdr *)(void *)((char *)ph + aux[AT_PHENT]))
				if (ph->p_type == PT_PHDR) app.base = (unsigned char *)aux[AT_PHDR] - ph->p_vaddr, have = 1;
			ph = (Phdr *)aux[AT_PHDR];
			for (i = 0; !have && i < aux[AT_PHNUM]; i++, ph = (Phdr *)(void *)((char *)ph + aux[AT_PHENT]))
				if (ph->p_type == PT_LOAD && !ph->p_offset)   /* no PT_PHDR */
					app.base = (unsigned char *)aux[AT_PHDR] - 64 - ph->p_vaddr, have = 1;
			ph = (Phdr *)aux[AT_PHDR];
			for (i = 0; i < aux[AT_PHNUM]; i++, ph = (Phdr *)(void *)((char *)ph + aux[AT_PHENT]))
				if (ph->p_type == PT_INTERP) interp = (const char *)(app.base + ph->p_vaddr);
		}
		if (interp && strlen(interp) < sizeof ldso_dir) {
			char *sl;
			strcpy(ldso_dir, interp);
			if ((sl = strrchr(ldso_dir, '/'))) *sl = 0;
			else strcpy(ldso_dir, ".");
			if (stat(interp, &st) == 0) { ldso.dev = st.st_dev; ldso.ino = st.st_ino; }
			ldso.name = (char *)interp;
		}
	}

	if (!aux[AT_BASE]) {
		/* run as a command: libc.so program [args...] */
		char buf[PATH_MAX];
		int fd;
		ssize_t l = readlink("/proc/self/exe", buf, sizeof buf - 1);
		if (l > 0) {
			struct stat st;
			char *sl;
			buf[l] = 0;
			if (stat(buf, &st) == 0) { ldso.dev = st.st_dev; ldso.ino = st.st_ino; }
			if ((sl = strrchr(buf, '/'))) { *sl = 0; strcpy(ldso_dir, buf); }
		}
		if (argc < 2) {
			dprintf(2, "usage: %s program [arguments...]\n"
			           "Runs a program linked dynamically against this C library.\n",
			        argv[0] ? argv[0] : "libc.so");
			_exit(1);
		}
		app.name = argv[1];
		__selfsym_file = argv[1];
		fd = open(argv[1], O_RDONLY | O_CLOEXEC);
		if (fd < 0) error("%s: %s", argv[1], strerror(errno));
		if (map_library(fd, &app) < 0) _exit(127);
		close(fd);
		{
			const Ehdr *eh = (const Ehdr *)(void *)app.map;
			entry = (size_t)app.base + eh->e_entry;
		}
		/* the program's view: argv without libc.so, its own auxv entries */
		sp[1] = (size_t)(argc - 1);
		sp++;
		argc--;
		argv++;
		for (i = 0; auxv[i]; i += 2) {
			if (auxv[i] == AT_PHDR) auxv[i + 1] = (size_t)app.phdr;
			else if (auxv[i] == AT_PHNUM) auxv[i + 1] = app.phnum;
			else if (auxv[i] == AT_ENTRY) auxv[i + 1] = entry;
			else if (auxv[i] == AT_BASE) auxv[i + 1] = (size_t)ldso.base;
		}
		if (app.tls.size) app.tls.id = ++tls_dyn_cnt;
	} else {
		Phdr *ph = (Phdr *)aux[AT_PHDR];
		app.name = argv[0] ? argv[0] : "";
		app.phdr = ph;
		app.phnum = aux[AT_PHNUM];
		app.phentsize = aux[AT_PHENT];
		for (i = 0; i < app.phnum; i++, ph = (Phdr *)(void *)((char *)ph + app.phentsize)) {
			if (ph->p_type == PT_DYNAMIC) app.dynv = (Dyn *)(void *)(app.base + ph->p_vaddr);
			else if (ph->p_type == PT_TLS) {
				app.tls.image = app.base + ph->p_vaddr;
				app.tls.len = ph->p_filesz;
				app.tls.size = ph->p_memsz;
				app.tls.align = ph->p_align ? ph->p_align : 1;
				app.tls.id = ++tls_dyn_cnt;
			} else if (ph->p_type == PT_GNU_RELRO) {
				app.relro_start = ph->p_vaddr & -page;
				app.relro_end = (ph->p_vaddr + ph->p_memsz) & -page;
			}
		}
		entry = aux[AT_ENTRY];
		if (!app.dynv) error("%s: the program has no dynamic section", app.name);
		decode_dyn(&app);
	}
	app.shortname = strrchr(app.name, '/') ? strrchr(app.name, '/') + 1 : app.name;
	app.global = 1;
	app.nodelete = 1;

	/* the list: the program, LD_PRELOAD, the dependencies; libc.so where
	 * it is first needed, or last */
	head = tail = &app;
	ldso.next = ldso.prev = 0;
	if (env_preload) load_preload(env_preload);
	load_deps(&app);
	if (!ldso.prev && tail != &ldso) {
		int linked = 0;
		for (p = head; p; p = p->next) if (p == &ldso) linked = 1;
		if (!linked) { ldso.prev = tail; tail->next = &ldso; tail = &ldso; }
	}
	for (p = head; p; p = p->next) {
		p->global = 1;
		p->nodelete = 1;
		make_scope(p);
	}

	/* the static TLS: every module loaded now; ids in list order */
	tls_dyn_cnt = 0;
	for (p = head; p; p = p->next)
		if (p->tls.size) { p->tls.id = ++tls_dyn_cnt; add_static_tls(p); }
	__tls_layout();

	/* relocate: dependencies first (copy relocations read their data),
	 * libc.so again (the program may have copies of its variables) */
	/* debuggers (before the program's RELRO, which may hold .dynamic, is read-only) */
	_r_debug.r_version = 1;
	_r_debug.r_map = head;
	_r_debug.r_brk = _dl_debug_state;
	_r_debug.r_ldbase = ldso.base;
	{
		Dyn *v;
		for (v = app.dynv; v->d_tag; v++)
			if (v->d_tag == DT_DEBUG) v->d_val = (uint64_t)(uintptr_t)&_r_debug;
	}
	for (p = tail; p; p = p->prev)
		if (p != &app) relocate(p, p == &ldso);
	relocate(&app, 0);
	_dl_debug_state();

	/* constructors, when __libc_start_main calls dl_init: in dependency order */
	for (p = tail; p; p = p->prev) queue_init(p, 0);
	queue_init(&app, 0);
	__libc.dl_init = dl_init;
	__libc.dl_fini = dl_fini;
	__libc.tls_get_new = tls_get_new;
	__libc.tls_free = tls_free;
	runtime = 1;

	/* the real thread pointer and static TLS */
	__init_main_tls();
	errno = 0;
	__dls_jump(entry, sp);
}

/* ---- <dlfcn.h> ---- */

static void dl_lock(void)
{
	int me = __pthread_self()->tid;
	if (lk_owner == me) { lk_count++; return; }
	LOCK(&lk);
	lk_owner = me;
	lk_count = 1;
}

static void dl_unlock(void)
{
	if (--lk_count) return;
	lk_owner = 0;
	UNLOCK(&lk);
}

static struct dso *addr2dso(size_t a)
{
	struct dso *p;
	for (p = head; p; p = p->next) {
		if (p->map && a - (size_t)p->map < p->map_len) return p;
		if (!p->map && p->phdr) {
			Phdr *ph = p->phdr;
			size_t i;
			for (i = 0; i < p->phnum; i++, ph = (Phdr *)(void *)((char *)ph + p->phentsize))
				if (ph->p_type == PT_LOAD && a - ((size_t)p->base + ph->p_vaddr) < ph->p_memsz)
					return p;
		}
	}
	return 0;
}

static int valid_handle(void *h)
{
	struct dso *p;
	for (p = head; p; p = p->next) if (p == h) return 1;
	return 0;
}

void *dlopen(const char *file, int mode)
{
	struct dso *p, *orig_tail;
	int how = mode & (RTLD_LAZY | RTLD_NOW);
	if (how != RTLD_LAZY && how != RTLD_NOW) {
		error("dlopen: invalid mode");
		return 0;
	}
	if (!file) return head;
	dl_lock();
	orig_tail = tail;
	if (mode & RTLD_NOLOAD) {
		const char *sn = strrchr(file, '/') ? strrchr(file, '/') + 1 : file;
		for (p = head; p; p = p->next)
			if ((p->shortname && !strcmp(p->shortname, sn)) || !strcmp(p->name, file)) break;
		if (!p) {
			int i;
			for (i = 0; builtin_names[i]; i++) if (!strcmp(file, builtin_names[i])) p = &ldso;
		}
		if (!p) { dl_unlock(); return 0; }
		goto done;
	}
	p = load_library(file, &app);
	if (!p) goto fail;
	if (load_deps(p) < 0) goto fail;
	{
		struct dso *q;
		for (q = orig_tail->next; q; q = q->next) if (make_scope(q) < 0) goto fail;
		if (make_scope(p) < 0) goto fail;
		/* new objects with TLS: dynamic TLS */
		for (q = orig_tail->next; q; q = q->next) q->static_tls = 0;
		/* relocate the new objects, dependencies first */
		for (q = tail; q != orig_tail; q = q->prev)
			if (!q->relocated && relocate(q, 0) < 0) goto fail;
		if (mode & RTLD_GLOBAL) {
			size_t i;
			for (i = 0; i < p->nscope; i++) p->scope[i]->global = 1;
		}
		if (queue_init(p, 0) < 0) goto fail;
	}
	_r_debug.r_state = 0;
	_dl_debug_state();
done:
	p->refcnt++;
	run_init(saved_argc, saved_argv, saved_envp);
	dl_unlock();
	return p;
fail:
	/* forget the objects this call loaded: none of them ran code */
	{
		struct dso *q = orig_tail->next, *n;
		orig_tail->next = 0;
		tail = orig_tail;
		for (; q; q = n) {
			n = q->next;
			unmap_dso(q);
			free(q->deps);
			free(q->scope);
			free(q);
		}
		/* their TLS ids */
		{
			size_t max = 0;
			for (q = head; q; q = q->next) if (q->tls.size && q->tls.id > max) max = q->tls.id;
			tls_dyn_cnt = max;
		}
	}
	dl_unlock();
	return 0;
}

int dlclose(void *h)
{
	int r = 0;
	dl_lock();
	if (!valid_handle(h)) {
		error("dlclose: invalid handle");
		r = -1;
	} else if (((struct dso *)h)->refcnt > 0)
		((struct dso *)h)->refcnt--;
	dl_unlock();
	return r;
}

char *dlerror(void)
{
	struct __pthread *t = __pthread_self();
	const char *m = t->dlerror;
	t->dlerror = 0;
	return (char *)m;
}

static void *sym_value(struct def d)
{
	if ((d.sym->st_info & 15) == STT_TLS) {
		unsigned long v[2] = { d.dso->tls.id, d.sym->st_value };
		return __tls_get_addr(v);
	}
	return (void *)sym_addr(d);
}

void *__dlsym(void *__RESTRICT h, const char *__RESTRICT name, void *ra)
{
	struct def d = { 0, 0 };
	uint32_t gh = gnu_hash(name), sh = sysv_hash(name);
	struct dso *p;
	size_t i;
	void *r = 0;
	dl_lock();
	if (h == RTLD_DEFAULT) {
		d = find_sym(name, 0, 0, 0);
		if (!d.sym) {
			struct dso *c = addr2dso((size_t)ra);
			if (c && !c->global) d = find_sym(name, c->scope, c->nscope, 0);
		}
	} else if (h == RTLD_NEXT) {
		/* the objects after the caller: in the global scope, then in its own */
		struct dso *c = addr2dso((size_t)ra);
		if (c) for (p = c->next; p && !d.sym; p = p->next)
			if (p->global && (d.sym = lookup(p, name, gh, sh))) d.dso = p;
		if (c && !d.sym) for (i = 1; i < c->nscope && !d.sym; i++)
			if ((d.sym = lookup(c->scope[i], name, gh, sh))) d.dso = c->scope[i];
	} else if (!valid_handle(h)) {
		error("dlsym: invalid handle");
		dl_unlock();
		return 0;
	} else {
		p = h;
		if (p == &app) {
			/* the program: the global scope, as glibc */
			d = find_sym(name, 0, 0, 0);
		} else {
			for (i = 0; i < p->nscope && !d.sym; i++)
				if ((d.sym = lookup(p->scope[i], name, gh, sh))) d.dso = p->scope[i];
			if (!d.sym && p == &ldso) d.sym = lookup(&ldso, name, gh, sh), d.dso = &ldso;
		}
	}
	if (d.sym) r = sym_value(d);
	else if (h == RTLD_DEFAULT || h == &app) {
		/* the program's own symbols outside its dynamic symbol table */
		r = __selfsym_lookup(name);
	}
	if (!r && !d.sym) error("dlsym: %s: symbol not found", name);
	dl_unlock();
	return r;
}

void *dlsym(void *__RESTRICT h, const char *__RESTRICT name)
{
	return __dlsym(h, name, __builtin_return_address(0));
}

int dladdr(const void *__RESTRICT addr, Dl_info_t *__RESTRICT info)
{
	struct dso *p;
	size_t a = (size_t)addr, i, n, best_v = 0;
	Sym *best = 0;
	dl_lock();
	p = addr2dso(a);
	if (!p) {
		dl_unlock();
		error("dladdr: address not in a loaded object");
		return 0;
	}
	n = count_syms(p);
	for (i = 1; i < n; i++) {
		Sym *s = &p->syms[i];
		size_t v;
		int t = s->st_info & 15;
		if (!s->st_shndx || !s->st_name || t == STT_TLS || !(1 << t & OK_TYPES)) continue;
		v = (size_t)p->base + s->st_value;
		if (v <= a && (!best || v > best_v)) best = s, best_v = v;
	}
	if (p == &app) {
		Dl_info_t si;
		if (__selfsym_addr(addr, &si) && si.dli_saddr && (!best || (size_t)si.dli_saddr > best_v)) {
			*info = si;
			info->dli_fname = app.name;
			dl_unlock();
			return 1;
		}
	}
	info->dli_fname = p->name;
	info->dli_fbase = p->map ? (void *)p->map : (void *)p->base;
	if (p == &app && !p->map) {
		/* the lowest address of the program's segments */
		Phdr *ph = p->phdr;
		size_t lo = SIZE_MAX;
		for (i = 0; i < p->phnum; i++, ph = (Phdr *)(void *)((char *)ph + p->phentsize))
			if (ph->p_type == PT_LOAD && ((size_t)p->base + ph->p_vaddr) < lo)
				lo = ((size_t)p->base + ph->p_vaddr) & -page;
		info->dli_fbase = (void *)lo;
	}
	info->dli_sname = best ? p->strings + best->st_name : 0;
	info->dli_saddr = best ? (void *)best_v : 0;
	dl_unlock();
	return 1;
}
