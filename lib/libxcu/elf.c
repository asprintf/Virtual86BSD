/*
 * elf.c - reading ELF object files (see elf.h)
 */

#include "elf.h"

#include <stdlib.h>
#include <string.h>

bool elf_is(const unsigned char *d, size_t n)
{
    return n >= 16 && d[0] == 0x7f && d[1] == 'E' && d[2] == 'L' && d[3] == 'F';
}

uint64_t elf_get(const struct elf *e, const unsigned char *p, int bytes)
{
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++)
        v |= (uint64_t)p[e->big ? bytes - 1 - i : i] << (8 * i);
    return v;
}

void elf_put(const struct elf *e, unsigned char *p, int bytes, uint64_t v)
{
    for (int i = 0; i < bytes; i++)
        p[e->big ? bytes - 1 - i : i] = (unsigned char)(v >> (8 * i));
}

/* a NUL-terminated string at offset off of section s, or "" */
static const char *str_at(const struct elf *e, const struct elf_section *s, uint64_t off)
{
    if (s->type == SHT_NOBITS || s->offset > e->size || s->size > e->size - s->offset || off >= s->size)
        return "";
    const char *p = (const char *)e->data + s->offset + off;
    if (!memchr(p, '\0', (size_t)(s->size - off)))
        return "";
    return p;
}

bool elf_open(struct elf *e, const unsigned char *d, size_t n, const char **why)
{
    memset(e, 0, sizeof *e);
    e->data = d;
    e->size = n;
    if (!elf_is(d, n)) {
        *why = "not an ELF file";
        return false;
    }
    if ((d[4] != 1 && d[4] != 2) || (d[5] != 1 && d[5] != 2) || d[6] != 1) {
        *why = "an ELF class, byte order or version not known";
        return false;
    }
    e->is64 = d[4] == 2;
    e->big = d[5] == 2;
    size_t ehsize = e->is64 ? 64 : 52;
    if (n < ehsize) {
        *why = "truncated";
        return false;
    }
    e->type = (unsigned)elf_get(e, d + 16, 2);
    e->machine = (unsigned)elf_get(e, d + 18, 2);
    if (e->is64) {
        e->phoff = elf_get(e, d + 32, 8);
        e->shoff = elf_get(e, d + 40, 8);
        e->phentsize = (unsigned)elf_get(e, d + 54, 2);
        e->phnum = (unsigned)elf_get(e, d + 56, 2);
        e->shentsize = (unsigned)elf_get(e, d + 58, 2);
        e->nsec = (size_t)elf_get(e, d + 60, 2);
        e->shstrndx = (size_t)elf_get(e, d + 62, 2);
    } else {
        e->phoff = elf_get(e, d + 28, 4);
        e->shoff = elf_get(e, d + 32, 4);
        e->phentsize = (unsigned)elf_get(e, d + 42, 2);
        e->phnum = (unsigned)elf_get(e, d + 44, 2);
        e->shentsize = (unsigned)elf_get(e, d + 46, 2);
        e->nsec = (size_t)elf_get(e, d + 48, 2);
        e->shstrndx = (size_t)elf_get(e, d + 50, 2);
    }
    if (e->shoff == 0) {
        e->nsec = 0;
        return true;
    }
    size_t want = e->is64 ? 64 : 40;
    if (e->shentsize < want || e->shoff > n || e->shoff + e->shentsize > n) {
        *why = "the section headers are out of the file";
        return false;
    }
    /* more than SHN_LORESERVE sections: the count and the index of the
       names in section 0 */
    const unsigned char *s0 = d + e->shoff;
    if (e->nsec == 0)
        e->nsec = (size_t)elf_get(e, s0 + (e->is64 ? 32 : 20), e->is64 ? 8 : 4);
    if (e->shstrndx == SHN_XINDEX)
        e->shstrndx = (size_t)elf_get(e, s0 + (e->is64 ? 44 : 28), 4);
    if (e->nsec > (n - e->shoff) / e->shentsize) {
        *why = "the section headers are out of the file";
        return false;
    }
    e->sec = calloc(e->nsec ? e->nsec : 1, sizeof *e->sec);
    if (!e->sec) {
        *why = "out of memory";
        return false;
    }
    for (size_t i = 0; i < e->nsec; i++) {
        const unsigned char *p = d + e->shoff + i * e->shentsize;
        struct elf_section *s = &e->sec[i];
        s->name = (const char *)(uintptr_t)elf_get(e, p, 4);   /* the offset, for now */
        s->type = (uint32_t)elf_get(e, p + 4, 4);
        if (e->is64) {
            s->flags = elf_get(e, p + 8, 8);
            s->addr = elf_get(e, p + 16, 8);
            s->offset = elf_get(e, p + 24, 8);
            s->size = elf_get(e, p + 32, 8);
            s->link = (uint32_t)elf_get(e, p + 40, 4);
            s->info = (uint32_t)elf_get(e, p + 44, 4);
            s->addralign = elf_get(e, p + 48, 8);
            s->entsize = elf_get(e, p + 56, 8);
        } else {
            s->flags = elf_get(e, p + 8, 4);
            s->addr = elf_get(e, p + 12, 4);
            s->offset = elf_get(e, p + 16, 4);
            s->size = elf_get(e, p + 20, 4);
            s->link = (uint32_t)elf_get(e, p + 24, 4);
            s->info = (uint32_t)elf_get(e, p + 28, 4);
            s->addralign = elf_get(e, p + 32, 4);
            s->entsize = elf_get(e, p + 36, 4);
        }
        if (s->type != SHT_NOBITS && (s->offset > n || s->size > n - s->offset)) {
            free(e->sec);
            e->sec = NULL;
            *why = "a section is out of the file";
            return false;
        }
    }
    for (size_t i = 0; i < e->nsec; i++) {
        uint64_t off = (uint64_t)(uintptr_t)e->sec[i].name;
        e->sec[i].name = e->shstrndx < e->nsec ? str_at(e, &e->sec[e->shstrndx], off) : "";
    }
    return true;
}

void elf_close(struct elf *e)
{
    free(e->sec);
    e->sec = NULL;
}

size_t elf_find_symtab(const struct elf *e, uint32_t type)
{
    for (size_t i = 1; i < e->nsec; i++)
        if (e->sec[i].type == type)
            return i;
    return 0;
}

size_t elf_symsize(const struct elf *e)
{
    return e->is64 ? 24 : 16;
}

size_t elf_nsyms(const struct elf *e, size_t s)
{
    if (s == 0 || s >= e->nsec)
        return 0;
    return (size_t)(e->sec[s].size / elf_symsize(e));
}

/* the SHT_SYMTAB_SHNDX section of symtab s, or 0 */
static size_t shndx_section(const struct elf *e, size_t s)
{
    for (size_t i = 1; i < e->nsec; i++)
        if (e->sec[i].type == SHT_SYMTAB_SHNDX && e->sec[i].link == s)
            return i;
    return 0;
}

bool elf_symbol(const struct elf *e, size_t s, size_t i, struct elf_symbol *sym)
{
    if (i >= elf_nsyms(e, s))
        return false;
    const struct elf_section *st = &e->sec[s];
    const unsigned char *p = e->data + st->offset + i * elf_symsize(e);
    uint32_t name = (uint32_t)elf_get(e, p, 4);
    unsigned info, shndx;
    if (e->is64) {
        info = p[4];
        sym->other = p[5];
        shndx = (unsigned)elf_get(e, p + 6, 2);
        sym->value = elf_get(e, p + 8, 8);
        sym->size = elf_get(e, p + 16, 8);
    } else {
        sym->value = elf_get(e, p + 4, 4);
        sym->size = elf_get(e, p + 8, 4);
        info = p[12];
        sym->other = p[13];
        shndx = (unsigned)elf_get(e, p + 14, 2);
    }
    sym->bind = info >> 4;
    sym->type = info & 0xf;
    sym->shndx = shndx;
    if (shndx == SHN_XINDEX) {
        size_t x = shndx_section(e, s);
        if (x && (i + 1) * 4 <= e->sec[x].size)
            sym->shndx = (size_t)elf_get(e, e->data + e->sec[x].offset + 4 * i, 4);
    }
    sym->name = st->link < e->nsec ? str_at(e, &e->sec[st->link], name) : "";
    return true;
}
