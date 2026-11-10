/* Internal: the few ELF definitions the start-up code needs. */
#ifndef _INTERNAL_ELF_MIN_H
#define _INTERNAL_ELF_MIN_H
#include <stdint.h>

#define AT_PHDR    3
#define AT_PHENT   4
#define AT_PHNUM   5
#define AT_PAGESZ  6
#define AT_HWCAP   16
#define AT_SECURE  23
#define AT_RANDOM  25
#define AT_SYSINFO_EHDR 33

#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_PHDR    6
#define PT_TLS     7

typedef struct {
	uint32_t p_type, p_flags;
	uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

#endif
