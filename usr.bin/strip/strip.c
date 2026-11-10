/*
 * strip.c - strip: remove unnecessary information from strippable files
 * (POSIX.1-2024, XCU strip)
 *
 * Synopsis: strip file...
 *
 * ELF files (elf.c), 32- or 64-bit, in either byte order, and archives
 * of them (arch.c).  What is removed:
 *  - from executables and shared objects (files with program headers):
 *    the symbol table (SHT_SYMTAB, its string table, SHT_SYMTAB_SHNDX)
 *    and the debugging sections (.debug*, .zdebug*, .stab*, .line, and
 *    the relocations of these); what the program headers load is kept
 *    byte for byte at its offsets, the other sections kept are put after
 *    it, then the section headers;
 *  - from relocatable objects: the debugging sections (and their
 *    relocations), the local symbols that no relocation uses (the
 *    symbols of the source files among them), and the undefined symbols
 *    that no relocation uses; the defined global and weak symbols, which
 *    linking needs, stay.  The file is laid out again: the
 *    sections in their order, aligned, then the section headers; the
 *    symbols, relocations and section groups are renumbered;
 *  - from archives: the same from each object member; the archive's
 *    symbol table is made again.
 * The result replaces the file (a new file in its directory, renamed over
 * it), with the file's permissions.
 *
 * Choices where the standard leaves room:
 *  - The names of the sections removed stay in the section name table.
 *  - A file that is neither an ELF file nor an archive, or that cannot be
 *    read as one, is reported and left as it is (status 1); the other
 *    operands are still stripped.  Members of an archive that are not
 *    ELF files are kept as they are.
 *  - A file with nothing to remove is written again all the same (but an
 *    executable is not laid out anew).
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "arch.h"
#include "elf.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int status;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("strip: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static void *xcalloc(size_t n, size_t s)
{
    void *p = calloc(n ? n : 1, s);
    if (!p) {
        fputs("strip: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

/* ---- output buffer ---- */

struct out {
    unsigned char *p;
    size_t n, cap;
};

static void grow(struct out *o, size_t n)
{
    if (n > o->cap) {
        size_t nc = n * 2 + 4096;
        unsigned char *np = realloc(o->p, nc);
        if (!np) {
            fputs("strip: out of memory\n", stderr);
            exit(1);
        }
        memset(np + o->cap, 0, nc - o->cap);
        o->p = np;
        o->cap = nc;
    }
    if (n > o->n)
        o->n = n;
}

static void put_at(struct out *o, size_t off, const void *s, size_t n)
{
    grow(o, off + n);
    memcpy(o->p + off, s, n);
}

static size_t align(size_t v, uint64_t a)
{
    if (a <= 1)
        return v;
    return (size_t)((v + a - 1) / a * a);
}

/* ---- which sections go ---- */

static bool debug_name(const char *n)
{
    return strncmp(n, ".debug", 6) == 0 || strncmp(n, ".zdebug", 7) == 0 || strncmp(n, ".stab", 5) == 0
           || strcmp(n, ".line") == 0 || strncmp(n, ".gnu.debuglto_", 14) == 0;
}

/* the sections removed: true in gone[] */
static void choose(const struct elf *e, bool *gone, bool symbols)
{
    for (size_t i = 1; i < e->nsec; i++) {
        const struct elf_section *s = &e->sec[i];
        if (!(s->flags & SHF_ALLOC) && debug_name(s->name))
            gone[i] = true;
        if (symbols && (s->type == SHT_SYMTAB || s->type == SHT_SYMTAB_SHNDX))
            gone[i] = true;
    }
    if (symbols) {
        /* the string table of the symbol table, unless something kept
           uses it */
        for (size_t i = 1; i < e->nsec; i++) {
            if (e->sec[i].type != SHT_SYMTAB)
                continue;
            size_t st = e->sec[i].link;
            if (st == 0 || st >= e->nsec || (e->sec[st].flags & SHF_ALLOC) || st == e->shstrndx)
                continue;
            bool used = false;
            for (size_t k = 1; k < e->nsec; k++)
                if (!gone[k] && k != st && e->sec[k].link == st)
                    used = true;
            if (!used)
                gone[st] = true;
        }
    }
    /* relocations of sections removed */
    for (size_t i = 1; i < e->nsec; i++) {
        const struct elf_section *s = &e->sec[i];
        if ((s->type == SHT_REL || s->type == SHT_RELA) && s->info > 0 && s->info < e->nsec && gone[s->info])
            gone[i] = true;
        if ((s->type == SHT_REL || s->type == SHT_RELA) && !(s->flags & SHF_ALLOC) && s->link < e->nsec
            && gone[s->link] && s->link != 0)
            gone[i] = true;
    }
}

/* ---- the section header table ---- */

static void put_shdr(const struct elf *e, unsigned char *p, const struct elf_section *s, uint32_t name)
{
    elf_put(e, p, 4, name);
    elf_put(e, p + 4, 4, s->type);
    if (e->is64) {
        elf_put(e, p + 8, 8, s->flags);
        elf_put(e, p + 16, 8, s->addr);
        elf_put(e, p + 24, 8, s->offset);
        elf_put(e, p + 32, 8, s->size);
        elf_put(e, p + 40, 4, s->link);
        elf_put(e, p + 44, 4, s->info);
        elf_put(e, p + 48, 8, s->addralign);
        elf_put(e, p + 56, 8, s->entsize);
    } else {
        elf_put(e, p + 8, 4, s->flags);
        elf_put(e, p + 12, 4, s->addr);
        elf_put(e, p + 16, 4, s->offset);
        elf_put(e, p + 20, 4, s->size);
        elf_put(e, p + 24, 4, s->link);
        elf_put(e, p + 28, 4, s->info);
        elf_put(e, p + 32, 4, s->addralign);
        elf_put(e, p + 36, 4, s->entsize);
    }
}

/* the name offsets of the sections, from the original headers */
static uint32_t name_off(const struct elf *e, size_t i)
{
    return (uint32_t)elf_get(e, e->data + e->shoff + i * e->shentsize, 4);
}

/* write the section headers of the kept sections at the end of o, and
   the ELF header's fields for them */
static void finish(const struct elf *e, struct out *o, struct elf_section *ns, const size_t *newidx, size_t nkept,
                   const size_t *oldof)
{
    size_t shentsize = e->is64 ? 64 : 40;
    size_t shoff = align(o->n, e->is64 ? 8 : 4);
    grow(o, shoff + shentsize * nkept);
    for (size_t k = 0; k < nkept; k++) {
        struct elf_section s = ns[k];
        size_t old = oldof[k];
        if (k == 0) {
            memset(&s, 0, sizeof s);
            if (nkept >= SHN_LORESERVE)
                s.size = nkept;
            size_t shstr = newidx[e->shstrndx];
            if (shstr >= SHN_LORESERVE)
                s.link = (uint32_t)shstr;
        }
        put_shdr(e, o->p + shoff + k * shentsize, &s, k ? name_off(e, old) : 0);
    }
    size_t shstr = newidx[e->shstrndx];
    unsigned char *h = o->p;
    if (e->is64) {
        elf_put(e, h + 40, 8, shoff);
        elf_put(e, h + 58, 2, shentsize);
        elf_put(e, h + 60, 2, nkept >= SHN_LORESERVE ? 0 : nkept);
        elf_put(e, h + 62, 2, shstr >= SHN_LORESERVE ? SHN_XINDEX : shstr);
    } else {
        elf_put(e, h + 32, 4, shoff);
        elf_put(e, h + 46, 2, shentsize);
        elf_put(e, h + 48, 2, nkept >= SHN_LORESERVE ? 0 : nkept);
        elf_put(e, h + 50, 2, shstr >= SHN_LORESERVE ? SHN_XINDEX : shstr);
    }
}

static uint32_t map_link(const size_t *newidx, size_t nsec, uint32_t v)
{
    return v < nsec ? (uint32_t)newidx[v] : 0;
}

/* ---- executables and shared objects ---- */

static bool strip_exec(const struct elf *e, struct out *o)
{
    bool *gone = xcalloc(e->nsec, sizeof *gone);
    choose(e, gone, true);
    /* what the program headers load, and the headers themselves */
    size_t keep = e->is64 ? 64 : 52;
    if (e->phoff + (uint64_t)e->phnum * e->phentsize > keep)
        keep = (size_t)(e->phoff + (uint64_t)e->phnum * e->phentsize);
    for (unsigned i = 0; i < e->phnum; i++) {
        const unsigned char *p = e->data + e->phoff + (size_t)i * e->phentsize;
        uint64_t off = e->is64 ? elf_get(e, p + 8, 8) : elf_get(e, p + 4, 4);
        uint64_t fsz = e->is64 ? elf_get(e, p + 32, 8) : elf_get(e, p + 16, 4);
        if (off + fsz > keep)
            keep = (size_t)(off + fsz);
    }
    for (size_t i = 1; i < e->nsec; i++)
        if ((e->sec[i].flags & SHF_ALLOC) && e->sec[i].type != SHT_NOBITS
            && e->sec[i].offset + e->sec[i].size > keep)
            keep = (size_t)(e->sec[i].offset + e->sec[i].size);
    if (keep > e->size)
        keep = e->size;
    put_at(o, 0, e->data, keep);
    size_t *newidx = xcalloc(e->nsec, sizeof *newidx);
    size_t *oldof = xcalloc(e->nsec, sizeof *oldof);
    struct elf_section *ns = xcalloc(e->nsec, sizeof *ns);
    size_t nk = 0;
    for (size_t i = 0; i < e->nsec; i++)
        if (!gone[i]) {
            newidx[i] = nk;
            oldof[nk] = i;
            ns[nk++] = e->sec[i];
        }
    for (size_t k = 1; k < nk; k++) {
        struct elf_section *s = &ns[k];
        if (s->type != SHT_NOBITS && s->offset + s->size > keep) {
            /* not in what is kept: after it */
            size_t off = align(o->n, s->addralign);
            put_at(o, off, e->data + s->offset, (size_t)s->size);
            s->offset = off;
        }
        s->link = map_link(newidx, e->nsec, s->link);
        if (s->type == SHT_REL || s->type == SHT_RELA || (s->flags & SHF_INFO_LINK))
            s->info = map_link(newidx, e->nsec, s->info);
    }
    finish(e, o, ns, newidx, nk, oldof);
    free(gone);
    free(newidx);
    free(oldof);
    free(ns);
    return true;
}

/* ---- relocatable objects ---- */

static uint64_t rel_sym(const struct elf *e, uint64_t info)
{
    return e->is64 ? info >> 32 : info >> 8;
}

static uint64_t rel_info(const struct elf *e, uint64_t info, uint64_t sym)
{
    return e->is64 ? (sym << 32) | (info & 0xffffffffu) : (sym << 8) | (info & 0xff);
}

static bool strip_rel(const struct elf *e, struct out *o, const char *name)
{
    bool *gone = xcalloc(e->nsec, sizeof *gone);
    choose(e, gone, false);
    size_t st = elf_find_symtab(e, SHT_SYMTAB);
    size_t nsyms = elf_nsyms(e, st);
    size_t symsz = elf_symsize(e);
    /* the symbols used: by the relocations and the groups kept */
    bool *used = xcalloc(nsyms, sizeof *used);
    for (size_t i = 1; i < e->nsec; i++) {
        const struct elf_section *s = &e->sec[i];
        if (gone[i] || s->link != st)
            continue;
        if (s->type == SHT_REL || s->type == SHT_RELA) {
            size_t es = s->type == SHT_RELA ? (e->is64 ? 24 : 12) : (e->is64 ? 16 : 8);
            for (uint64_t off = 0; off + es <= s->size; off += es) {
                uint64_t info = elf_get(e, e->data + s->offset + off + (e->is64 ? 8 : 4), e->is64 ? 8 : 4);
                uint64_t y = rel_sym(e, info);
                if (y < nsyms)
                    used[y] = true;
            }
        } else if (s->type == SHT_GROUP && s->info < nsyms) {
            used[s->info] = true;
        }
    }
    /* the symbols kept: the null one, the locals used, then the others */
    size_t *symmap = xcalloc(nsyms, sizeof *symmap);
    size_t *order = xcalloc(nsyms, sizeof *order);
    size_t nkept = 0, nlocal = 0;
    for (int pass = 0; pass < 2 && st; pass++)
        for (size_t i = 0; i < nsyms; i++) {
            struct elf_symbol y;
            if (!elf_symbol(e, st, i, &y))
                continue;
            bool local = y.bind == STB_LOCAL;
            if ((pass == 0) != local)
                continue;
            /* an undefined global symbol that no relocation uses is not
               needed either (as GNU strip --strip-unneeded) */
            bool keep = i == 0 || used[i] || (!local && y.shndx != SHN_UNDEF);
            if (keep && y.shndx > 0 && y.shndx < e->nsec && gone[y.shndx] && local)
                keep = false;
            if (!keep)
                continue;
            symmap[i] = nkept;
            order[nkept++] = i;
            if (local)
                nlocal = nkept;
        }
    /* the sections kept, and their new numbers */
    size_t *newidx = xcalloc(e->nsec, sizeof *newidx);
    size_t *oldof = xcalloc(e->nsec, sizeof *oldof);
    struct elf_section *ns = xcalloc(e->nsec, sizeof *ns);
    size_t nk = 0;
    for (size_t i = 0; i < e->nsec; i++)
        if (!gone[i]) {
            newidx[i] = nk;
            oldof[nk] = i;
            ns[nk++] = e->sec[i];
        }
    /* the new symbol and string tables */
    size_t strsec = st ? e->sec[st].link : 0;
    struct out syms = { 0 }, strs = { 0 }, shndx = { 0 };
    if (st) {
        put_at(&strs, 0, "", 1);
        for (size_t k = 0; k < nkept; k++) {
            struct elf_symbol y;
            elf_symbol(e, st, order[k], &y);
            unsigned char ent[24];
            memcpy(ent, e->data + e->sec[st].offset + order[k] * symsz, symsz);
            size_t nameoff = 0;
            if (y.name[0]) {
                nameoff = strs.n;
                put_at(&strs, strs.n, y.name, strlen(y.name) + 1);
            }
            /* the section: renumbered; SHN_ABS and SHN_COMMON as they are */
            unsigned raw = (unsigned)elf_get(e, ent + (e->is64 ? 6 : 14), 2);
            unsigned stored = raw;
            uint32_t xs = 0;
            if (raw != SHN_UNDEF && (raw < SHN_LORESERVE || raw == SHN_XINDEX) && y.shndx < e->nsec) {
                size_t sh = newidx[y.shndx];
                if (sh >= SHN_LORESERVE) {
                    stored = SHN_XINDEX;
                    xs = (uint32_t)sh;
                } else {
                    stored = (unsigned)sh;
                }
            }
            elf_put(e, ent, 4, nameoff);
            elf_put(e, ent + (e->is64 ? 6 : 14), 2, stored);
            put_at(&syms, syms.n, ent, symsz);
            unsigned char x[4];
            elf_put(e, x, 4, xs);
            put_at(&shndx, shndx.n, x, 4);
        }
    }
    /* the output: the header, the sections in order, the section headers */
    size_t ehsize = e->is64 ? 64 : 52;
    put_at(o, 0, e->data, ehsize);
    /* no program headers in a relocatable object */
    if (e->is64) {
        elf_put(e, o->p + 32, 8, 0);
        elf_put(e, o->p + 54, 2, 0);
        elf_put(e, o->p + 56, 2, 0);
    } else {
        elf_put(e, o->p + 28, 4, 0);
        elf_put(e, o->p + 42, 2, 0);
        elf_put(e, o->p + 44, 2, 0);
    }
    for (size_t k = 1; k < nk; k++) {
        struct elf_section *s = &ns[k];
        size_t old = oldof[k];
        const unsigned char *src = e->data + s->offset;
        size_t size = (size_t)s->size;
        unsigned char *buf = NULL;
        if (old == st && st) {
            src = syms.p;
            size = syms.n;
            s->info = (uint32_t)nlocal;
        } else if (old == strsec && st) {
            src = strs.p;
            size = strs.n;
        } else if (s->type == SHT_SYMTAB_SHNDX && s->link == st) {
            src = shndx.p;
            size = shndx.n;
        } else if ((s->type == SHT_REL || s->type == SHT_RELA) && s->link == st && st) {
            buf = xmalloc(size);
            memcpy(buf, src, size);
            size_t es = s->type == SHT_RELA ? (e->is64 ? 24 : 12) : (e->is64 ? 16 : 8);
            int w = e->is64 ? 8 : 4;
            for (size_t off = 0; off + es <= size; off += es) {
                uint64_t info = elf_get(e, buf + off + (size_t)w, w);
                uint64_t y = rel_sym(e, info);
                elf_put(e, buf + off + (size_t)w, w, rel_info(e, info, y < nsyms ? symmap[y] : 0));
            }
            src = buf;
        } else if (s->type == SHT_GROUP) {
            /* the flags word, then the members: renumbered, those removed
               left out */
            buf = xmalloc(size);
            size_t n = 0;
            for (size_t off = 0; off + 4 <= size; off += 4) {
                uint64_t v = elf_get(e, src + off, 4);
                if (off > 0) {
                    if (v >= e->nsec || gone[v])
                        continue;
                    v = newidx[v];
                }
                elf_put(e, buf + n, 4, v);
                n += 4;
            }
            src = buf;
            size = n;
            if (s->info < nsyms)
                s->info = (uint32_t)symmap[s->info];
        }
        s->link = map_link(newidx, e->nsec, s->link);
        if (s->type == SHT_REL || s->type == SHT_RELA || (s->flags & SHF_INFO_LINK))
            s->info = map_link(newidx, e->nsec, s->info);
        size_t off = align(o->n, s->addralign);
        if (s->type != SHT_NOBITS)
            put_at(o, off, src, size);
        s->offset = off;
        s->size = size;
        if (s->type == SHT_NOBITS)
            s->size = e->sec[old].size;
        free(buf);
    }
    (void)name;
    finish(e, o, ns, newidx, nk, oldof);
    free(gone);
    free(used);
    free(symmap);
    free(order);
    free(newidx);
    free(oldof);
    free(ns);
    free(syms.p);
    free(strs.p);
    free(shndx.p);
    return true;
}

/* strip ELF data into o: false with a message */
static bool strip_elf(const char *what, const unsigned char *data, size_t size, struct out *o)
{
    struct elf e;
    const char *why;
    if (!elf_open(&e, data, size, &why)) {
        fprintf(stderr, "strip: %s: %s\n", what, why);
        return false;
    }
    bool ok;
    if (e.nsec == 0) {
        put_at(o, 0, data, size);       /* no sections: nothing to remove */
        ok = true;
    } else if (e.type == ET_REL) {
        ok = strip_rel(&e, o, what);
    } else if (e.type == ET_EXEC || e.type == ET_DYN || e.phnum > 0) {
        ok = strip_exec(&e, o);
    } else {
        fprintf(stderr, "strip: %s: not a relocatable object, executable or shared object\n", what);
        ok = false;
    }
    elf_close(&e);
    return ok;
}

static bool replace_file(const char *path, const unsigned char *d, size_t n, const struct stat *st)
{
    char tmp[PATH_MAX];
    const char *b = strrchr(path, '/');
    int dl = b ? (int)(b - path + 1) : 0;
    if (snprintf(tmp, sizeof tmp, "%.*sstrip.XXXXXX", dl, path) >= (int)sizeof tmp) {
        fprintf(stderr, "strip: %s: name too long\n", path);
        return false;
    }
    int fd = mkstemp(tmp);
    if (fd < 0) {
        fprintf(stderr, "strip: %s: %s\n", tmp, strerror(errno));
        return false;
    }
    size_t off = 0;
    bool ok = true;
    while (off < n) {
        ssize_t k = write(fd, d + off, n - off);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            ok = false;
            break;
        }
        off += (size_t)k;
    }
    if (ok && fchmod(fd, st->st_mode & 07777) != 0)
        ok = false;
    if (close(fd) != 0)
        ok = false;
    if (!ok || rename(tmp, path) != 0) {
        fprintf(stderr, "strip: %s: %s\n", path, strerror(errno));
        unlink(tmp);
        return false;
    }
    return true;
}

static void strip_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "strip: %s: %s\n", path, strerror(errno));
        status = 1;
        return;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "strip: %s: not a regular file\n", path);
        status = 1;
        return;
    }
    size_t size;
    unsigned char *d = ar_slurp(path, &size);
    if (!d) {
        fprintf(stderr, "strip: %s: %s\n", path, strerror(errno));
        status = 1;
        return;
    }
    if (ar_is(d, size)) {
        struct archive a;
        const char *why;
        if (!ar_parse(&a, d, size, &why)) {
            fprintf(stderr, "strip: %s: %s\n", path, why);
            status = 1;
            free(d);
            return;
        }
        bool ok = true;
        for (size_t i = 0; i < a.n && ok; i++) {
            if (!elf_is(a.m[i].data, a.m[i].size))
                continue;
            size_t l = strlen(path) + strlen(a.m[i].name) + 4;
            char *what = xmalloc(l);
            snprintf(what, l, "%s[%s]", path, a.m[i].name);
            struct out o = { 0 };
            ok = strip_elf(what, a.m[i].data, a.m[i].size, &o);
            free(what);
            if (ok) {
                free(a.m[i].data);
                a.m[i].data = o.p;
                a.m[i].size = o.n;
            } else {
                free(o.p);
            }
        }
        if (ok) {
            size_t n;
            unsigned char *out = ar_build(&a, &n, (long long)time(NULL));
            if (!out) {
                fputs("strip: out of memory\n", stderr);
                ok = false;
            } else {
                ok = replace_file(path, out, n, &st);
                free(out);
            }
        }
        if (!ok)
            status = 1;
        ar_free(&a);
    } else if (elf_is(d, size)) {
        struct out o = { 0 };
        if (!strip_elf(path, d, size, &o) || !replace_file(path, o.p, o.n, &st))
            status = 1;
        free(o.p);
    } else {
        fprintf(stderr, "strip: %s: not an object file or an archive\n", path);
        status = 1;
    }
    free(d);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0)
        i++;
    else if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        fprintf(stderr, "strip: unknown option: %s\nusage: strip file...\n", argv[i]);
        return 2;
    }
    if (i >= argc) {
        fputs("usage: strip file...\n", stderr);
        return 2;
    }
    for (; i < argc; i++)
        strip_file(argv[i]);
    return status;
}
