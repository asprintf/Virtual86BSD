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
 * cxref.c - cxref: generate a C-language program cross-reference table
 * (POSIX.1-2024, XCU cxref)
 *
 * Synopsis: cxref [-cs] [-o file] [-w num] [-D name[=def]]... [-I dir]...
 *                 [-U name]... file...
 *
 * Each file is preprocessed (cpp.c, with -D, -I and -U in their order)
 * and parsed (cparse.c).  The symbols: the macros (#define), and the
 * names the C declares and uses: functions, objects (external, static and
 * automatic, and parameters), typedef names and enumeration constants;
 * not the members and tags of structures, unions and enumerations.  A
 * reference is where a name is written: a line number, "*" before it
 * where it is declared (#define for a macro), "=" before it where it is
 * assigned to (=, op=, ++, --); for a macro, where it is expanded or
 * tested (#ifdef, #ifndef, defined, #undef).  The references made by the
 * expansion of a macro are on the line where the macro is used.
 *
 * The listing: a line "SYMBOL FILE FUNCTION LINE", then a line for each
 * symbol, file and function: the symbol, the file the references are in,
 * the function they are in ("--" outside functions, and for the names of
 * functions themselves), and the references, in the order of the lines;
 * sorted by symbol (LC_COLLATE), file and function.  The columns are as
 * wide as their widest entries; the references are wrapped so that no
 * line is wider than -w num columns (80 by default, and when num is less
 * than 51), the lines after the first indented to their column.
 *
 * Without -c, a listing for each file, after a line with the file's name
 * (left out with -s); with -c, one listing of all the files.  -o file:
 * the listing to that file instead of the standard output.
 *
 * Choices where the standard leaves room:
 *  - The symbols of the headers a file includes are listed too, with the
 *    headers' names in the FILE column.
 *  - A name declared in several scopes of a function is one symbol there.
 *  - A file that cannot be read: a diagnostic, status 1; the others are
 *    listed.  Problems in the C (an #include not found, an #error) are
 *    warnings.  Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L

#include "cparse.h"
#include "cpp.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ref {
    const char *name, *file, *func;
    int line;
    char flag;                  /* '*', '=' or 0 */
    bool macro;
    size_t seq;
};

static struct ref *refs;
static size_t nrefs, caprefs;

/* the functions defined in the file being read: where their bodies are */
struct range {
    const char *name, *file;
    int from, to;
};

static struct range *ranges;
static size_t nranges, capranges;

/* the names known as functions */
#define NHASH 4099
static struct fname {
    const char *name;
    struct fname *next;
} *funcs[NHASH];

static unsigned hash(const char *s)
{
    unsigned h = 5381;
    for (; *s; s++)
        h = h * 33 + (unsigned char)*s;
    return h % NHASH;
}

static void add_func(const char *name)
{
    unsigned h = hash(name);
    for (struct fname *f = funcs[h]; f; f = f->next)
        if (strcmp(f->name, name) == 0)
            return;
    struct fname *f = xmalloc(sizeof *f);
    f->name = name;
    f->next = funcs[h];
    funcs[h] = f;
}

static bool is_func(const char *name)
{
    for (struct fname *f = funcs[hash(name)]; f; f = f->next)
        if (strcmp(f->name, name) == 0)
            return true;
    return false;
}

static void clear_funcs(void)
{
    for (size_t i = 0; i < NHASH; i++)
        while (funcs[i]) {
            struct fname *f = funcs[i];
            funcs[i] = f->next;
            free(f);
        }
}

static void add_ref(const char *name, const char *file, const char *func, int line, char flag, bool macro)
{
    if (nrefs == caprefs) {
        caprefs = caprefs ? 2 * caprefs : 1024;
        refs = xrealloc(refs, caprefs * sizeof *refs);
    }
    refs[nrefs] = (struct ref){ name, file, func, line, flag, macro, nrefs };
    nrefs++;
}

static void on_decl(void *arg, const struct cdecl *d)
{
    (void)arg;
    if (d->kind == CD_TAG)
        return;
    if (d->kind == CD_FUNCDEF || d->kind == CD_FUNC)
        add_func(d->name->text);
    if (d->kind == CD_FUNCDEF && d->end) {
        if (nranges == capranges) {
            capranges = capranges ? 2 * capranges : 64;
            ranges = xrealloc(ranges, capranges * sizeof *ranges);
        }
        ranges[nranges++] = (struct range){ d->name->text, d->name->file, d->name->oline, d->end->oline };
    }
    add_ref(d->name->text, d->name->file, d->func, d->name->oline, '*', false);
}

static void on_ref(void *arg, const struct cref *r)
{
    (void)arg;
    if (r->call && r->kind < 0)
        add_func(r->name->text);
    add_ref(r->name->text, r->name->file, r->func, r->name->oline, r->assign ? '=' : 0, false);
}

static void on_define(void *arg, const struct token *name)
{
    (void)arg;
    add_ref(name->text, name->file, NULL, name->oline, '*', true);
}

static void on_use(void *arg, const struct token *name)
{
    (void)arg;
    add_ref(name->text, name->file, NULL, name->oline, 0, true);
}

/* the function of the macro references of a file */
static void macro_functions(size_t from)
{
    for (size_t i = from; i < nrefs; i++) {
        struct ref *r = &refs[i];
        if (!r->macro)
            continue;
        for (size_t k = 0; k < nranges; k++)
            if (strcmp(ranges[k].file, r->file) == 0 && ranges[k].from <= r->line && r->line <= ranges[k].to) {
                r->func = ranges[k].name;
                break;
            }
    }
    nranges = 0;
}

/* functions: their own names are not in a function */
static void function_names(size_t from)
{
    for (size_t i = from; i < nrefs; i++)
        if (!refs[i].macro && is_func(refs[i].name))
            refs[i].func = NULL;
    clear_funcs();
}

static int cmp_ref(const void *a, const void *b)
{
    const struct ref *x = a, *y = b;
    int c = strcoll(x->name, y->name);
    if (c == 0)
        c = strcmp(x->name, y->name);
    if (c == 0)
        c = strcmp(x->file, y->file);
    if (c == 0 && x->func != y->func) {
        if (!x->func)
            c = -1;
        else if (!y->func)
            c = 1;
        else {
            c = strcoll(x->func, y->func);
            if (c == 0)
                c = strcmp(x->func, y->func);
        }
    }
    if (c == 0)
        c = (x->line > y->line) - (x->line < y->line);
    if (c == 0)
        c = (x->seq > y->seq) - (x->seq < y->seq);
    return c;
}

static bool same_row(const struct ref *x, const struct ref *y)
{
    if (strcmp(x->name, y->name) != 0 || strcmp(x->file, y->file) != 0)
        return false;
    if (!x->func || !y->func)
        return x->func == y->func;
    return strcmp(x->func, y->func) == 0;
}

static FILE *out;
static long width = 80;

static void field(const char *s, size_t w)
{
    size_t l = strlen(s);
    fputs(s, out);
    if (l + 1 > w)
        w = l + 1;
    for (size_t i = l; i < w; i++)
        putc(' ', out);
}

static void listing(size_t from, size_t to)
{
    struct ref *v = refs + from;
    size_t n = to - from;
    qsort(v, n, sizeof *v, cmp_ref);
    /* the widths of the columns: the widest entries, but within a third
       of the width for each of the first three */
    size_t ws = 7, wf = 5, wn = 9;
    for (size_t i = 0; i < n; i++) {
        size_t l = strlen(v[i].name) + 2;
        if (l > ws)
            ws = l;
        l = strlen(v[i].file) + 2;
        if (l > wf)
            wf = l;
        l = (v[i].func ? strlen(v[i].func) : 2) + 2;
        if (l > wn)
            wn = l;
    }
    size_t cap = (size_t)(width - 10) / 3;
    if (ws > cap)
        ws = cap;
    if (wf > cap)
        wf = cap;
    if (wn > cap)
        wn = cap;
    field("SYMBOL", ws);
    field("FILE", wf);
    field("FUNCTION", wn);
    fputs("LINE\n", out);
    for (size_t i = 0; i < n;) {
        size_t j = i;
        while (j < n && same_row(&v[i], &v[j]))
            j++;
        /* the columns, as wide as given, or wider where an entry is */
        size_t col = 0;
        field(v[i].name, ws);
        col += strlen(v[i].name) + 1 > ws ? strlen(v[i].name) + 1 : ws;
        field(v[i].file, wf);
        col += strlen(v[i].file) + 1 > wf ? strlen(v[i].file) + 1 : wf;
        const char *fn = v[i].func ? v[i].func : "--";
        field(fn, wn);
        col += strlen(fn) + 1 > wn ? strlen(fn) + 1 : wn;
        size_t start = col, pos = col;
        bool first = true;
        for (size_t k = i; k < j; k++) {
            /* the same reference twice: once */
            if (k > i && v[k].line == v[k - 1].line && v[k].flag == v[k - 1].flag)
                continue;
            char buf[32];
            if (v[k].flag)
                snprintf(buf, sizeof buf, "%c%d", v[k].flag, v[k].line);
            else
                snprintf(buf, sizeof buf, "%d", v[k].line);
            size_t l = strlen(buf);
            if (!first && pos + 1 + l > (size_t)width) {
                putc('\n', out);
                for (size_t s = 0; s < start; s++)
                    putc(' ', out);
                pos = start;
                first = true;
            }
            if (!first) {
                putc(' ', out);
                pos++;
            }
            fputs(buf, out);
            pos += l;
            first = false;
        }
        putc('\n', out);
        i = j;
    }
}

static void usage(void)
{
    fputs("usage: cxref [-cs] [-o file] [-w num] [-D name[=def]]... [-I dir]... [-U name]... file...\n",
          stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "cxref";
    struct cpp *cpp = cpp_new();
    bool o_c = false, o_s = false;
    const char *outfile = NULL;
    int c;
    while ((c = getopt(argc, argv, "cso:w:D:I:U:")) != -1) {
        switch (c) {
        case 'c': o_c = true; break;
        case 's': o_s = true; break;
        case 'o': outfile = optarg; break;
        case 'w': {
            char *end;
            errno = 0;
            long v = strtol(optarg, &end, 10);
            if (end == optarg || *end || errno)
                usage();
            width = v < 51 ? 80 : v;
            break;
        }
        case 'D': cpp_define(cpp, optarg); break;
        case 'U': cpp_undef(cpp, optarg); break;
        case 'I': cpp_include_dir(cpp, optarg); break;
        default: usage();
        }
    }
    if (optind >= argc)
        usage();
    out = stdout;
    if (outfile && !(out = fopen(outfile, "w"))) {
        fprintf(stderr, "cxref: %s: %s\n", outfile, strerror(errno));
        return 1;
    }
    struct cpp_hooks ch = { .define = on_define, .use = on_use };
    cpp_set_hooks(cpp, &ch);
    struct cparse_hooks ph = { on_decl, on_ref, NULL };
    int status = 0;
    bool any = false;
    for (int i = optind; i < argc; i++) {
        size_t from = nrefs;
        struct token *t = cpp_run(cpp, argv[i], argv[i]);
        if (!t) {
            fprintf(stderr, "cxref: %s: %s\n", argv[i], strerror(errno));
            status = 1;
            continue;
        }
        cparse(t, &ph);
        macro_functions(from);
        function_names(from);
        if (!o_c) {
            if (any)
                putc('\n', out);
            if (!o_s)
                fprintf(out, "%s\n\n", argv[i]);
            listing(from, nrefs);
            any = true;
        }
    }
    if (o_c && nrefs > 0)
        listing(0, nrefs);
    if (fflush(out) != 0 || ferror(out) || (out != stdout && fclose(out) != 0)) {
        fprintf(stderr, "cxref: %s: %s\n", outfile ? outfile : "standard output", strerror(errno));
        return 1;
    }
    return status;
}
