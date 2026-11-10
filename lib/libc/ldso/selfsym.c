/* The program's own ELF symbol table (from /proc/self/exe): what dlsym
 * and dladdr use for the program in a static link, and in a dynamic link
 * for its symbols that are not in the dynamic symbol table (a program
 * not linked with -rdynamic). */
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pthread_impl.h"
#include "elf_min.h"
#include "lock.h"

typedef struct {
	unsigned char e_ident[16];
	uint16_t e_type, e_machine;
	uint32_t e_version;
	uint64_t e_entry, e_phoff, e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	uint32_t sh_name, sh_type;
	uint64_t sh_flags, sh_addr, sh_offset, sh_size;
	uint32_t sh_link, sh_info;
	uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;

typedef struct {
	uint32_t st_name;
	unsigned char st_info, st_other;
	uint16_t st_shndx;
	uint64_t st_value, st_size;
} Elf64_Sym;

#define SHT_SYMTAB 2
#define STT_OBJECT 1
#define STT_FUNC 2
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

unsigned long __getauxval(unsigned long);

static struct {
	int state;                 /* 0 not looked at, 1 done */
	char path[PATH_MAX];
	uintptr_t lo, hi, bias;
	const Elf64_Sym *sym;
	size_t nsym;
	const char *str;
	size_t strsz;
} self;
static volatile int lock;
const char *__selfsym_file;   /* the program, if not /proc/self/exe (libc.so) */
/* the program's address range, and its symbol table if it has one */
static void load(void)
{
	int have_bias = 0;
	const Elf64_Phdr *ph = (const Elf64_Phdr *)__getauxval(AT_PHDR);
	size_t phnum = __getauxval(AT_PHNUM), i;
	const Elf64_Ehdr *eh;
	const Elf64_Shdr *sh;
	struct stat st;
	ssize_t n;
	void *map;
	int fd;

	if (self.state)
		return;
	self.state = 1;
	self.lo = UINTPTR_MAX;
	for (i = 0; ph && i < phnum; i++)
		if (ph[i].p_type == PT_PHDR)
			self.bias = (uintptr_t)ph - ph[i].p_vaddr, have_bias = 1;
	for (i = 0; ph && !have_bias && i < phnum; i++)   /* a static PIE: no PT_PHDR */
		if (ph[i].p_type == PT_LOAD && !ph[i].p_offset)
			self.bias = (uintptr_t)ph - 64 - ph[i].p_vaddr, have_bias = 1;
	for (i = 0; ph && i < phnum; i++)
		if (ph[i].p_type == PT_LOAD) {
			uintptr_t a = (ph[i].p_vaddr & -4096UL) + self.bias, b = ph[i].p_vaddr + ph[i].p_memsz + self.bias;
			if (a < self.lo) self.lo = a;
			if (b > self.hi) self.hi = b;
		}
	if (__selfsym_file) {   /* run as "libc.so program": not /proc/self/exe */
		n = (ssize_t)strlen(__selfsym_file);
		if (n >= (ssize_t)sizeof self.path) n = sizeof self.path - 1;
		memcpy(self.path, __selfsym_file, (size_t)n);
	} else if ((n = readlink("/proc/self/exe", self.path, sizeof self.path - 1)) < 0)
		n = 0;
	self.path[n] = 0;
	if ((fd = open(__selfsym_file ? __selfsym_file : "/proc/self/exe", O_RDONLY | O_CLOEXEC)) < 0)
		return;
	if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof *eh
	    || (map = mmap(0, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		close(fd);
		return;
	}
	close(fd);
	eh = map;
	if (memcmp(eh->e_ident, "\177ELF\2", 5) || eh->e_shentsize != sizeof *sh
	    || eh->e_shoff + (uint64_t)eh->e_shnum * sizeof *sh > (uint64_t)st.st_size)
		return;
	sh = (const Elf64_Shdr *)((const char *)map + eh->e_shoff);
	for (i = 0; i < eh->e_shnum; i++)
		if (sh[i].sh_type == SHT_SYMTAB && sh[i].sh_link < eh->e_shnum) {
			const Elf64_Shdr *s = &sh[sh[i].sh_link];
			if (sh[i].sh_offset + sh[i].sh_size > (uint64_t)st.st_size
			    || s->sh_offset + s->sh_size > (uint64_t)st.st_size || !s->sh_size)
				return;
			self.sym = (const Elf64_Sym *)((const char *)map + sh[i].sh_offset);
			self.nsym = sh[i].sh_size / sizeof *self.sym;
			self.str = (const char *)map + s->sh_offset;
			self.strsz = s->sh_size;
			return;
		}
}

static void init(void)
{
	LOCK(&lock);
	load();
	UNLOCK(&lock);
}

static const char *symname(const Elf64_Sym *s)
{
	if (s->st_name >= self.strsz || !memchr(self.str + s->st_name, 0, self.strsz - s->st_name))
		return 0;
	return self.str + s->st_name;
}

static int usable(const Elf64_Sym *s)
{
	int type = s->st_info & 15;
	return s->st_shndx != 0 && (type == STT_FUNC || type == STT_OBJECT) && s->st_name;
}

/* the program's symbol name (global before weak), or 0 */
void *__selfsym_lookup(const char *name)
{
	const Elf64_Sym *s, *weak = 0;
	const char *n;
	size_t i;
	init();
	for (i = 1; i < self.nsym; i++) {
		s = &self.sym[i];
		if ((s->st_info >> 4) == STB_LOCAL || !usable(s) || !(n = symname(s)) || strcmp(n, name))
			continue;
		if ((s->st_info >> 4) == STB_GLOBAL)
			return (void *)(s->st_value + self.bias);
		if (!weak)
			weak = s;
	}
	return weak ? (void *)(weak->st_value + self.bias) : 0;
}

int __selfsym_present(void)
{
	init();
	return self.sym != 0;
}

/* dladdr for an address in the program: 0 if it is not */
int __selfsym_addr(const void *addr, Dl_info_t *info)
{
	uintptr_t a = (uintptr_t)addr, v;
	const Elf64_Sym *s, *best = 0;
	size_t i;
	init();
	if (a < self.lo || a >= self.hi)
		return 0;
	for (i = 1; i < self.nsym; i++) {
		s = &self.sym[i];
		if (!usable(s) || !symname(s))
			continue;
		v = s->st_value + self.bias;
		if (v > a)
			continue;
		if (!best || v > best->st_value + self.bias
		    || (v == best->st_value + self.bias && (best->st_info >> 4) == STB_LOCAL && (s->st_info >> 4) != STB_LOCAL))
			best = s;
	}
	info->dli_fname = self.path;
	info->dli_fbase = (void *)self.lo;
	info->dli_sname = best ? symname(best) : 0;
	info->dli_saddr = best ? (void *)(best->st_value + self.bias) : 0;
	return 1;
}
