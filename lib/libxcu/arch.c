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
 * arch.c - archives (see arch.h)
 *
 * The symbol table lists, for each object member in order, its symbols
 * that other objects can be linked against (global, weak and unique
 * binding, defined or common), each with the offset of its member's
 * header: a count, the offsets and the names, as big-endian 32-bit
 * words, or 64-bit ones in a "/SYM64/" table when the archive is larger
 * than 4 GiB, as GNU ar writes them.
 */

#define _POSIX_C_SOURCE 202405L

#include "arch.h"
#include "elf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool ar_is(const unsigned char *d, size_t n)
{
    return n >= 8 && memcmp(d, "!<arch>\n", 8) == 0;
}

static long long field(const unsigned char *p, int len, int base, bool *ok)
{
    char buf[24];
    memcpy(buf, p, (size_t)len);
    buf[len] = '\0';
    char *s = buf;
    while (*s == ' ')
        s++;
    if (!*s)
        return 0;
    char *e;
    errno = 0;
    long long v = strtoll(s, &e, base);
    while (*e == ' ')
        e++;
    if (*e || errno)
        *ok = false;
    return v;
}

static bool add(struct archive *a, const struct ar_member *m)
{
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 16;
        struct ar_member *nm = realloc(a->m, nc * sizeof *nm);
        if (!nm)
            return false;
        a->m = nm;
        a->cap = nc;
    }
    a->m[a->n++] = *m;
    return true;
}

bool ar_parse(struct archive *a, const unsigned char *d, size_t n, const char **why)
{
    memset(a, 0, sizeof *a);
    if (!ar_is(d, n)) {
        *why = "not an archive";
        return false;
    }
    const unsigned char *longnames = NULL;
    size_t longsize = 0;
    size_t off = 8;
    while (off < n) {
        if (n - off < 60 || d[off + 58] != '`' || d[off + 59] != '\n') {
            *why = "a member header is not valid";
            goto bad;
        }
        const unsigned char *h = d + off;
        bool ok = true;
        long long size = field(h + 48, 10, 10, &ok);
        if (!ok || size < 0 || (unsigned long long)size > n - off - 60) {
            *why = "a member is out of the archive";
            goto bad;
        }
        const unsigned char *data = h + 60;
        char raw[17];
        memcpy(raw, h, 16);
        raw[16] = '\0';
        size_t dsize = (size_t)size;
        off += 60 + dsize + (dsize & 1);
        if (strncmp(raw, "/               ", 16) == 0 || strncmp(raw, "/SYM64/         ", 16) == 0
            || strncmp(raw, "__.SYMDEF", 9) == 0) {
            a->had_symtab = true;
            continue;
        }
        if (strncmp(raw, "//              ", 16) == 0) {
            longnames = data;
            longsize = dsize;
            continue;
        }
        struct ar_member m;
        memset(&m, 0, sizeof m);
        if (raw[0] == '/' && raw[1] >= '0' && raw[1] <= '9') {
            /* a long name: its offset in "//", ended by "/\n" */
            long long lo = field(h + 1, 15, 10, &ok);
            if (!ok || !longnames || lo < 0 || (size_t)lo >= longsize) {
                *why = "a long name is not in the table of names";
                goto bad;
            }
            const unsigned char *s = longnames + lo, *e = s;
            while ((size_t)(e - longnames) < longsize && *e != '\n' && !(*e == '/' && (e + 1 == longnames + longsize || e[1] == '\n')))
                e++;
            m.name = strndup((const char *)s, (size_t)(e - s));
        } else if (strncmp(raw, "#1/", 3) == 0) {
            /* BSD: the name after the header, counted in the size */
            long long nl = field(h + 3, 13, 10, &ok);
            if (!ok || nl < 0 || (size_t)nl > dsize) {
                *why = "a long name is out of its member";
                goto bad;
            }
            m.name = strndup((const char *)data, (size_t)nl);
            data += nl;
            dsize -= (size_t)nl;
        } else {
            int l = 16;
            while (l > 0 && raw[l - 1] == ' ')
                l--;
            if (l > 0 && raw[l - 1] == '/')
                l--;
            m.name = strndup(raw, (size_t)l);
        }
        if (!m.name) {
            *why = "out of memory";
            goto bad;
        }
        m.mtime = field(h + 16, 12, 10, &ok);
        m.uid = (long)field(h + 28, 6, 10, &ok);
        m.gid = (long)field(h + 34, 6, 10, &ok);
        m.mode = (unsigned long)field(h + 40, 8, 8, &ok);
        if (!ok) {
            free(m.name);
            *why = "a member header is not valid";
            goto bad;
        }
        m.size = dsize;
        m.data = malloc(dsize ? dsize : 1);
        if (!m.data) {
            free(m.name);
            *why = "out of memory";
            goto bad;
        }
        memcpy(m.data, data, dsize);
        if (!add(a, &m)) {
            free(m.name);
            free(m.data);
            *why = "out of memory";
            goto bad;
        }
    }
    return true;
bad:
    ar_free(a);
    return false;
}

void ar_free(struct archive *a)
{
    for (size_t i = 0; i < a->n; i++) {
        free(a->m[i].name);
        free(a->m[i].data);
    }
    free(a->m);
    memset(a, 0, sizeof *a);
}

/* ---- writing ---- */

struct buf {
    unsigned char *p;
    size_t n, cap;
    bool nomem;
};

static void put(struct buf *b, const void *s, size_t n)
{
    if (b->n + n > b->cap) {
        size_t nc = (b->n + n) * 2 + 64;
        unsigned char *np = realloc(b->p, nc);
        if (!np) {
            b->nomem = true;
            return;
        }
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
}

/* a header field: the value left-justified, padded with spaces; a value
   that does not fit is written as 0 */
static void hfield(char *dst, int width, const char *val)
{
    size_t l = strlen(val);
    if ((int)l > width) {
        val = "0";
        l = 1;
    }
    memset(dst, ' ', (size_t)width);
    memcpy(dst, val, l);
}

static void header(struct buf *b, const char *name, long long mtime, long uid, long gid, unsigned long mode,
                   size_t size, bool special)
{
    char h[61], v[32];
    hfield(h, 16, name);
    snprintf(v, sizeof v, "%lld", mtime);
    hfield(h + 16, 12, v);
    snprintf(v, sizeof v, "%ld", uid);
    hfield(h + 28, 6, v);
    snprintf(v, sizeof v, "%ld", gid);
    hfield(h + 34, 6, v);
    snprintf(v, sizeof v, special ? "%lu" : "%lo", mode);
    hfield(h + 40, 8, v);
    snprintf(v, sizeof v, "%zu", size);
    hfield(h + 48, 10, v);
    h[58] = '`';
    h[59] = '\n';
    put(b, h, 60);
}

struct sym {
    const char *name;
    size_t member;
};

/* the linkable symbols of an object member: appended to *syms; true if
   it is an object file */
static bool member_symbols(const struct ar_member *m, size_t idx, struct sym **syms, size_t *n, size_t *cap,
                           struct elf *keep)
{
    const char *why;
    if (!elf_open(keep, m->data, m->size, &why))
        return false;
    size_t s = elf_find_symtab(keep, SHT_SYMTAB);
    size_t ns = elf_nsyms(keep, s);
    for (size_t i = 1; i < ns; i++) {
        struct elf_symbol y;
        if (!elf_symbol(keep, s, i, &y) || !y.name[0])
            continue;
        if (y.bind != STB_GLOBAL && y.bind != STB_WEAK && y.bind != STB_GNU_UNIQUE)
            continue;
        if (y.shndx == SHN_UNDEF)
            continue;
        if (*n == *cap) {
            *cap = *cap ? *cap * 2 : 64;
            struct sym *ns2 = realloc(*syms, *cap * sizeof **syms);
            if (!ns2)
                return true;
            *syms = ns2;
        }
        (*syms)[(*n)++] = (struct sym){ y.name, idx };
    }
    return true;
}

unsigned char *ar_build(const struct archive *a, size_t *size, long long symtab_time)
{
    /* the long names */
    struct buf names = { 0 };
    size_t *nameoff = calloc(a->n ? a->n : 1, sizeof *nameoff);
    struct elf *elves = calloc(a->n ? a->n : 1, sizeof *elves);
    if (!nameoff || !elves) {
        free(nameoff);
        free(elves);
        return NULL;
    }
    for (size_t i = 0; i < a->n; i++) {
        size_t l = strlen(a->m[i].name);
        if (l > 15) {
            nameoff[i] = names.n;
            put(&names, a->m[i].name, l);
            put(&names, "/\n", 2);
        }
    }
    if (names.n & 1)
        put(&names, "\n", 1);
    /* the symbols */
    struct sym *syms = NULL;
    size_t nsyms = 0, capsyms = 0;
    bool objects = false;
    for (size_t i = 0; i < a->n; i++)
        if (member_symbols(&a->m[i], i, &syms, &nsyms, &capsyms, &elves[i]))
            objects = true;
    size_t strsize = 0;
    for (size_t i = 0; i < nsyms; i++)
        strsize += strlen(syms[i].name) + 1;
    /* the offsets of the members, for a table of 32- or of 64-bit words */
    int w = 4;
    size_t *offs = calloc(a->n ? a->n : 1, sizeof *offs);
    for (int pass = 0; pass < 2 && offs; pass++) {
        size_t tab = objects ? 60 + (size_t)w + (size_t)w * nsyms + strsize : 0;
        tab += tab & 1;
        size_t off = 8 + tab + (names.n ? 60 + names.n : 0);
        for (size_t i = 0; i < a->n; i++) {
            offs[i] = off;
            off += 60 + a->m[i].size + (a->m[i].size & 1);
        }
        if (w == 4 && off > 0xffffffffu)
            w = 8;
        else
            break;
    }
    struct buf b = { 0 };
    put(&b, "!<arch>\n", 8);
    if (objects && offs) {
        size_t tsize = (size_t)w + (size_t)w * nsyms + strsize;
        header(&b, w == 4 ? "/" : "/SYM64/", symtab_time, 0, 0, 0, tsize + (tsize & 1), true);
        unsigned char word[8];
        for (int k = 0; k < w; k++)
            word[k] = (unsigned char)((uint64_t)nsyms >> (8 * (w - 1 - k)));
        put(&b, word, (size_t)w);
        for (size_t i = 0; i < nsyms; i++) {
            for (int k = 0; k < w; k++)
                word[k] = (unsigned char)((uint64_t)offs[syms[i].member] >> (8 * (w - 1 - k)));
            put(&b, word, (size_t)w);
        }
        for (size_t i = 0; i < nsyms; i++)
            put(&b, syms[i].name, strlen(syms[i].name) + 1);
        if (tsize & 1)
            put(&b, "", 1);
    }
    if (names.n) {
        header(&b, "//", 0, 0, 0, 0, names.n, true);
        /* the uid, gid and mode fields of "//" are blank */
        memset(b.p + b.n - 60 + 16, ' ', 32);
        put(&b, names.p, names.n);
    }
    for (size_t i = 0; i < a->n; i++) {
        const struct ar_member *m = &a->m[i];
        char nm[32];
        if (strlen(m->name) > 15)
            snprintf(nm, sizeof nm, "/%zu", nameoff[i]);
        else
            snprintf(nm, sizeof nm, "%s/", m->name);
        header(&b, nm, m->mtime, m->uid, m->gid, m->mode, m->size, false);
        put(&b, m->data, m->size);
        if (m->size & 1)
            put(&b, "\n", 1);
    }
    for (size_t i = 0; i < a->n; i++)
        elf_close(&elves[i]);
    free(elves);
    free(syms);
    free(offs);
    free(nameoff);
    free(names.p);
    if (b.nomem || names.nomem || !offs) {
        free(b.p);
        return NULL;
    }
    *size = b.n;
    return b.p;
}

unsigned char *ar_slurp(const char *path, size_t *size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        int e = errno;
        close(fd);
        errno = e;
        return NULL;
    }
    if (S_ISDIR(st.st_mode)) {
        close(fd);
        errno = EISDIR;
        return NULL;
    }
    size_t cap = S_ISREG(st.st_mode) ? (size_t)st.st_size + 1 : 65536, n = 0;
    unsigned char *p = malloc(cap);
    if (!p) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }
    for (;;) {
        if (n == cap) {
            unsigned char *np = realloc(p, cap * 2);
            if (!np) {
                free(p);
                close(fd);
                errno = ENOMEM;
                return NULL;
            }
            p = np;
            cap *= 2;
        }
        ssize_t k = read(fd, p + n, cap - n);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            free(p);
            close(fd);
            errno = e;
            return NULL;
        }
        if (k == 0)
            break;
        n += (size_t)k;
    }
    close(fd);
    *size = n;
    return p;
}
