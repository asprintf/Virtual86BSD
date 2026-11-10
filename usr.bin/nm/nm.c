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
 * nm.c - nm: write the name list of an object file (POSIX.1-2024, XCU nm)
 *
 * Synopsis: nm [-APv] [-efox] [-g|-u] [-t format] file...
 *
 * The symbols are those of the symbol table (SHT_SYMTAB) of ELF object
 * files, executables and shared objects (elf.c), 32- or 64-bit, in
 * either byte order, and of the object members of archives (arch.c).
 * The type of a symbol:
 *     U  undefined                  A a  absolute
 *     C  common                     B b  in a section of no bits (bss)
 *     T t  in an executable section  D d  in a writable section
 *     R r  in a read-only section    N    a debugging section
 *     n    another section that is not loaded
 *     W w  weak (w: undefined)       V v  weak object (v: undefined)
 *     i  an indirect function        u    a unique global symbol
 * upper case for global symbols, lower case for local ones.
 *
 * The output: "%s%s %s %s %s\n", the -A prefix, the name, the type, the
 * value and the size, in decimal (-t d, the default without -P), octal
 * (-t o, -o) or hexadecimal (-t x, -x, the default with -P); sorted by
 * name in the collation of LC_COLLATE, or with -v by value (then name).
 *
 * Choices where the standard leaves room:
 *  - The default format is that of -P (with decimal numbers).
 *  - The symbols of sections (STT_SECTION) and of source files (STT_FILE)
 *    are written only with -f (-f: "redundant symbols"); -e writes the
 *    external and static symbols, which are all the others.
 *  - An undefined symbol has the value and size 0; the value of a common
 *    symbol is its size, as in BSD and GNU nm.
 *  - A file without a symbol table: "nm: FILE: no symbols" on the
 *    standard error, not an error.  A file that is neither an ELF file nor
 *    an archive is an error (status 1); so are members of an archive that
 *    are not object files: they are left out without a message.
 *  - The header line of a file or member is written as the standard says,
 *    without empty lines around it.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "arch.h"
#include "elf.h"

#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool o_A, o_P, o_v, o_e, o_f, o_g, o_u;
static char fmt;                /* d, o or x */
static int status;

struct sym {
    const char *name;
    char type;
    uint64_t value, size;
};

static char sym_type(const struct elf *e, const struct elf_symbol *s)
{
    bool global = s->bind != STB_LOCAL;
    char c;
    if (s->type == STT_GNU_IFUNC)
        return 'i';
    if (s->bind == STB_GNU_UNIQUE)
        return 'u';
    if (s->shndx == SHN_UNDEF) {
        if (s->bind == STB_WEAK)
            return s->type == STT_OBJECT ? 'v' : 'w';
        return 'U';
    }
    if (s->bind == STB_WEAK)
        return s->type == STT_OBJECT ? 'V' : 'W';
    if (s->shndx == SHN_COMMON || s->type == STT_COMMON)
        return 'C';
    if (s->shndx == SHN_ABS)
        c = 'a';
    else if (s->shndx >= e->nsec)
        c = '?';
    else {
        const struct elf_section *sec = &e->sec[s->shndx];
        if (strncmp(sec->name, ".debug", 6) == 0 || strncmp(sec->name, ".zdebug", 7) == 0)
            return 'N';
        else if (!(sec->flags & SHF_ALLOC))
            return 'n';
        else if (sec->type == SHT_NOBITS)
            c = 'b';
        else if (sec->flags & SHF_EXECINSTR)
            c = 't';
        else if (sec->flags & SHF_WRITE)
            c = 'd';
        else
            c = 'r';
    }
    if (c == '?')
        return c;
    return global ? (char)(c - 'a' + 'A') : c;
}

static int by_name(const void *a, const void *b)
{
    const struct sym *x = a, *y = b;
    int c = strcoll(x->name, y->name);
    if (c == 0)
        c = strcmp(x->name, y->name);
    if (c == 0)
        c = x->value < y->value ? -1 : x->value > y->value;
    return c;
}

static int by_value(const void *a, const void *b)
{
    const struct sym *x = a, *y = b;
    if (x->value != y->value)
        return x->value < y->value ? -1 : 1;
    return by_name(a, b);
}

static void put_num(uint64_t v)
{
    switch (fmt) {
    case 'o': printf("%" PRIo64, v); break;
    case 'x': printf("%" PRIx64, v); break;
    default: printf("%" PRIu64, v); break;
    }
}

/* the symbols of one object; prefix for -A ("" otherwise) */
static void object(const char *what, const unsigned char *data, size_t size, const char *prefix,
                   const char *header)
{
    struct elf e;
    const char *why;
    if (!elf_open(&e, data, size, &why)) {
        fprintf(stderr, "nm: %s: %s\n", what, why);
        status = 1;
        return;
    }
    size_t s = elf_find_symtab(&e, SHT_SYMTAB);
    if (header)
        printf("%s:\n", header);
    if (s == 0) {
        fflush(stdout);
        fprintf(stderr, "nm: %s: no symbols\n", what);
        elf_close(&e);
        return;
    }
    size_t n = elf_nsyms(&e, s), k = 0;
    struct sym *v = malloc(sizeof *v * (n ? n : 1));
    if (!v) {
        fputs("nm: out of memory\n", stderr);
        exit(1);
    }
    for (size_t i = 1; i < n; i++) {
        struct elf_symbol y;
        if (!elf_symbol(&e, s, i, &y))
            continue;
        bool redundant = y.type == STT_SECTION || y.type == STT_FILE;
        if (redundant && !o_f)
            continue;
        const char *name = y.name;
        if (y.type == STT_SECTION && !name[0] && y.shndx < e.nsec)
            name = e.sec[y.shndx].name;
        if (!name[0])
            continue;
        char t = y.type == STT_FILE ? 'f' : sym_type(&e, &y);
        bool undefined = y.shndx == SHN_UNDEF;
        if (o_u && !undefined)
            continue;
        if (o_g && y.bind == STB_LOCAL)
            continue;
        /* a common symbol's value is its size (as in BSD and GNU nm) */
        uint64_t value = undefined ? 0 : t == 'C' ? y.size : y.value;
        v[k++] = (struct sym){ name, t, value, undefined ? 0 : y.size };
    }
    qsort(v, k, sizeof *v, o_v ? by_value : by_name);
    for (size_t i = 0; i < k; i++) {
        printf("%s%s %c ", prefix, v[i].name, v[i].type);
        put_num(v[i].value);
        putchar(' ');
        put_num(v[i].size);
        putchar('\n');
    }
    free(v);
    elf_close(&e);
}

static void file(const char *path, bool several)
{
    size_t size;
    unsigned char *d = ar_slurp(path, &size);
    if (!d) {
        fprintf(stderr, "nm: %s: %s\n", path, strerror(errno));
        status = 1;
        return;
    }
    if (ar_is(d, size)) {
        struct archive a;
        const char *why;
        if (!ar_parse(&a, d, size, &why)) {
            fprintf(stderr, "nm: %s: %s\n", path, why);
            status = 1;
            free(d);
            return;
        }
        for (size_t i = 0; i < a.n; i++) {
            if (!elf_is(a.m[i].data, a.m[i].size))
                continue;
            size_t l = strlen(path) + strlen(a.m[i].name) + 8;
            char *what = malloc(l), *prefix = malloc(l);
            if (!what || !prefix) {
                fputs("nm: out of memory\n", stderr);
                exit(1);
            }
            snprintf(what, l, "%s[%s]", path, a.m[i].name);
            if (o_A)
                snprintf(prefix, l, "%s[%s]: ", path, a.m[i].name);
            else
                prefix[0] = '\0';
            object(what, a.m[i].data, a.m[i].size, prefix, o_A ? NULL : what);
            free(what);
            free(prefix);
        }
        ar_free(&a);
    } else if (elf_is(d, size)) {
        size_t l = strlen(path) + 4;
        char *prefix = malloc(l);
        if (!prefix) {
            fputs("nm: out of memory\n", stderr);
            exit(1);
        }
        if (o_A)
            snprintf(prefix, l, "%s: ", path);
        else
            prefix[0] = '\0';
        object(path, d, size, prefix, several && !o_A ? path : NULL);
        free(prefix);
    } else {
        fprintf(stderr, "nm: %s: not an object file or an archive\n", path);
        status = 1;
    }
    free(d);
}

static void usage(void)
{
    fputs("usage: nm [-APv] [-efox] [-g|-u] [-t format] file...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "APvefoxgut:")) != -1) {
        switch (c) {
        case 'A': o_A = true; break;
        case 'P': o_P = true; break;
        case 'v': o_v = true; break;
        case 'e': o_e = true; break;
        case 'f': o_f = true; break;
        case 'g': o_g = true; break;
        case 'u': o_u = true; break;
        case 'o': fmt = 'o'; break;
        case 'x': fmt = 'x'; break;
        case 't':
            if (strlen(optarg) != 1 || !strchr("dox", optarg[0]))
                usage();
            fmt = optarg[0];
            break;
        default: usage();
        }
    }
    if (o_g && o_u)
        usage();
    if (!fmt)
        fmt = o_P ? 'x' : 'd';
    (void)o_e;
    if (optind >= argc)
        usage();
    bool several = argc - optind > 1;
    for (int i = optind; i < argc; i++)
        file(argv[i], several);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "nm: standard output: %s\n", strerror(errno));
        return 1;
    }
    return status;
}
