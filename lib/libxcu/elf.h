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

/*
 * elf.h - reading ELF object files: the header, the sections, the
 * symbols; 32- and 64-bit, either byte order (shared by ar, nm, strip and
 * cflow)
 */

#ifndef ELF_H
#define ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* the values of the ELF specification used here */
enum {
    ET_REL = 1, ET_EXEC = 2, ET_DYN = 3,
    SHT_NULL = 0, SHT_PROGBITS = 1, SHT_SYMTAB = 2, SHT_STRTAB = 3, SHT_RELA = 4, SHT_HASH = 5,
    SHT_DYNAMIC = 6, SHT_NOTE = 7, SHT_NOBITS = 8, SHT_REL = 9, SHT_DYNSYM = 11, SHT_GROUP = 17,
    SHT_SYMTAB_SHNDX = 18,
    SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4, SHF_INFO_LINK = 0x40,
    SHN_UNDEF = 0, SHN_LORESERVE = 0xff00, SHN_ABS = 0xfff1, SHN_COMMON = 0xfff2, SHN_XINDEX = 0xffff,
    STB_LOCAL = 0, STB_GLOBAL = 1, STB_WEAK = 2, STB_GNU_UNIQUE = 10,
    STT_NOTYPE = 0, STT_OBJECT = 1, STT_FUNC = 2, STT_SECTION = 3, STT_FILE = 4, STT_COMMON = 5,
    STT_TLS = 6, STT_GNU_IFUNC = 10
};

struct elf_section {
    const char *name;
    uint32_t type;
    uint64_t flags, addr, offset, size, entsize, addralign;
    uint32_t link, info;
};

struct elf {
    const unsigned char *data;
    size_t size;
    bool is64, big;
    unsigned type;              /* e_type */
    unsigned machine;
    uint64_t shoff, phoff;
    unsigned phnum, phentsize, shentsize;
    struct elf_section *sec;    /* sec[0..nsec) */
    size_t nsec;
    size_t shstrndx;
};

struct elf_symbol {
    const char *name;
    uint64_t value, size;
    unsigned bind, type, other;
    size_t shndx;               /* the section index, SHN_XINDEX resolved */
};

/* is data an ELF file?  (the magic number only) */
bool elf_is(const unsigned char *data, size_t size);
/* read the header and the sections: false with *why set when the file is
   not a valid ELF file this can read */
bool elf_open(struct elf *e, const unsigned char *data, size_t size, const char **why);
void elf_close(struct elf *e);

/* the integers of the file */
uint64_t elf_get(const struct elf *e, const unsigned char *p, int bytes);
void elf_put(const struct elf *e, unsigned char *p, int bytes, uint64_t v);

/* the symbol table section of the given type (SHT_SYMTAB, SHT_DYNSYM):
   its index, or 0 */
size_t elf_find_symtab(const struct elf *e, uint32_t type);
/* the number of symbols of symtab section s, and symbol i of it */
size_t elf_nsyms(const struct elf *e, size_t s);
bool elf_symbol(const struct elf *e, size_t s, size_t i, struct elf_symbol *sym);
/* the size of a symbol table entry */
size_t elf_symsize(const struct elf *e);

#endif
