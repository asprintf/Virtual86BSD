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
 * ctags.c - ctags: create a tags file (POSIX.1-2024, XCU ctags)
 *
 * Synopsis: ctags [-a] [-f tagsfile] pathname...
 *           ctags -x pathname...
 *
 * The files are read as C (cpp.c, not preprocessed, and cparse.c); the
 * objects tagged are:
 *     function definitions             (main: M and the file's name)
 *     type definitions (typedef names)
 *     macros, with or without arguments (#define)
 *     structure, union and enumeration tags (where the members are listed)
 * The directives are taken out of the C: of each #if, #ifdef, #ifndef
 * the first group is read, the others (#elif, #else) are not, except
 * that a "#if 0" group is not read and the next one is; a #define is
 * tagged in any group.
 *
 * The tags file: "%s\t%s\t/^%s$/\n", identifier, file name, the line (the
 * whole line, "\" and "/" escaped), sorted by identifier in the order of
 * the POSIX locale (bytes), without duplicate lines; with -a, the lines
 * of the file before are kept (and sorted with the new ones).  -f -: the
 * standard output.
 *
 * -x: "%-16s %4d %-16s %s\n", the identifier, the line number, the file
 * name and the text of the line, sorted by identifier (LC_COLLATE), then
 * file name and line number.
 *
 * Choices where the standard leaves room:
 *  - The same identifier in several places is written for each (the
 *    lines are different); main is written only as M<file>.
 *  - Any file is read as C, whatever its suffix.
 *  - A file that cannot be read: a diagnostic, status 1; the tags of the
 *    other files are written.
 *  - -x with -a or -f, and no pathname, are usage errors (status 2).
 */

#define _POSIX_C_SOURCE 202405L

#include "cparse.h"
#include "cpp.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct tag {
    char *name;
    const char *file;
    int line;
    const char *text;
};

static struct tag *tags;
static size_t ntags, captags;

struct source {
    const char *path;
    char **lines;               /* lines[1..n] */
    size_t n;
};

static struct source *cur;

static void add_tag(const char *name, int line)
{
    if (ntags == captags) {
        captags = captags ? 2 * captags : 256;
        tags = xrealloc(tags, captags * sizeof *tags);
    }
    const char *text = line >= 1 && (size_t)line <= cur->n ? cur->lines[line] : "";
    char *n;
    if (strcmp(name, "main") == 0) {
        const char *b = strrchr(cur->path, '/');
        b = b ? b + 1 : cur->path;
        size_t l = strlen(b);
        if (l > 2 && strcmp(b + l - 2, ".c") == 0)
            l -= 2;
        n = xmalloc(l + 2);
        n[0] = 'M';
        memcpy(n + 1, b, l);
        n[l + 1] = '\0';
    } else {
        n = xstrdup(name);
    }
    tags[ntags++] = (struct tag){ n, cur->path, line, text };
}

static void on_decl(void *arg, const struct cdecl *d)
{
    (void)arg;
    switch (d->kind) {
    case CD_FUNCDEF:
    case CD_TYPEDEF:
        add_tag(d->name->text, d->name->oline);
        break;
    case CD_TAG:
        if (d->has_body)
            add_tag(d->name->text, d->name->oline);
        break;
    default:
        break;
    }
}

/* the lines of the file, as they are */
static bool read_lines(struct source *s)
{
    FILE *f = fopen(s->path, "r");
    if (!f)
        return false;
    size_t cap = 64;
    s->lines = xmalloc(cap * sizeof *s->lines);
    s->lines[0] = NULL;
    s->n = 0;
    char *line = NULL;
    size_t lcap = 0;
    ssize_t len;
    while ((len = getline(&line, &lcap, f)) >= 0) {
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (s->n + 2 > cap) {
            cap *= 2;
            s->lines = xrealloc(s->lines, cap * sizeof *s->lines);
        }
        s->lines[++s->n] = xstrndup(line, (size_t)len);
    }
    free(line);
    int err = ferror(f) ? errno : 0;
    fclose(f);
    if (err) {
        errno = err;
        return false;
    }
    return true;
}

static bool is_hash(const struct token *t)
{
    return t->bol && t->kind == CPP_PUNCT && strcmp(t->text, "#") == 0;
}

/* take the directives out; tag the macros */
static struct token *directives(struct token *t)
{
    struct group {
        bool skip;              /* this group is not read */
        bool kept;              /* a group of this #if was read */
        bool outer;             /* the #if is in a group not read */
    } *st = NULL;
    size_t n = 0, cap = 0;
    struct token head = { 0 }, *last = &head;
    while (t->kind != CPP_EOF) {
        bool skipping = n > 0 && (st[n - 1].skip || st[n - 1].outer);
        if (!is_hash(t)) {
            if (!skipping)
                last = last->next = t;
            t = t->next;
            continue;
        }
        struct token *d = t->next;
        if (!d->bol && d->kind == CPP_IDENT) {
            const char *s = d->text;
            if (strcmp(s, "define") == 0 && !d->next->bol && d->next->kind == CPP_IDENT) {
                add_tag(d->next->text, d->next->oline);
            } else if (strcmp(s, "if") == 0 || strcmp(s, "ifdef") == 0 || strcmp(s, "ifndef") == 0) {
                if (n == cap) {
                    cap = cap ? 2 * cap : 16;
                    st = xrealloc(st, cap * sizeof *st);
                }
                bool zero = strcmp(s, "if") == 0 && !d->next->bol && strcmp(d->next->text, "0") == 0 &&
                            d->next->next->bol;
                st[n] = (struct group){ zero, !zero, skipping };
                n++;
            } else if ((strcmp(s, "elif") == 0 || strcmp(s, "else") == 0) && n > 0) {
                bool zero = strcmp(s, "elif") == 0 && !d->next->bol && strcmp(d->next->text, "0") == 0 &&
                            d->next->next->bol;
                if (st[n - 1].kept || zero) {
                    st[n - 1].skip = true;
                } else {
                    st[n - 1].skip = false;
                    st[n - 1].kept = true;
                }
            } else if (strcmp(s, "endif") == 0 && n > 0) {
                n--;
            }
        }
        t = t->next;
        while (!t->bol)
            t = t->next;
    }
    last->next = t;
    free(st);
    return head.next;
}

static int by_tagline(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int by_xref(const void *a, const void *b)
{
    const struct tag *x = a, *y = b;
    int c = strcoll(x->name, y->name);
    if (c == 0)
        c = strcmp(x->name, y->name);
    if (c == 0)
        c = strcmp(x->file, y->file);
    if (c == 0)
        c = (x->line > y->line) - (x->line < y->line);
    return c;
}

static char *tag_line(const struct tag *t)
{
    size_t n = strlen(t->name) + strlen(t->file) + 2 * strlen(t->text) + 8;
    char *s = xmalloc(n), *q = s;
    q += sprintf(q, "%s\t%s\t/^", t->name, t->file);
    for (const char *p = t->text; *p; p++) {
        if (*p == '\\' || *p == '/')
            *q++ = '\\';
        *q++ = *p;
    }
    strcpy(q, "$/");
    return s;
}

static void usage(void)
{
    fputs("usage: ctags [-a] [-f tagsfile] pathname...\n       ctags -x pathname...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "ctags";
    bool o_a = false, o_x = false, o_f = false;
    const char *tagsfile = "tags";
    int c;
    while ((c = getopt(argc, argv, "af:x")) != -1) {
        switch (c) {
        case 'a': o_a = true; break;
        case 'f': tagsfile = optarg; o_f = true; break;
        case 'x': o_x = true; break;
        default: usage();
        }
    }
    if (optind >= argc || (o_x && (o_a || o_f)))
        usage();
    int status = 0;
    struct cparse_hooks h = { on_decl, NULL, NULL };
    for (int i = optind; i < argc; i++) {
        struct source *s = xcalloc(1, sizeof *s);
        s->path = argv[i];
        struct token *t;
        if (!read_lines(s) || !(t = cpp_tokenize(argv[i], argv[i]))) {
            fprintf(stderr, "ctags: %s: %s\n", argv[i], strerror(errno));
            status = 1;
            continue;
        }
        cur = s;
        t = directives(t);
        cparse(t, &h);
    }
    if (o_x) {
        if (ntags)
            qsort(tags, ntags, sizeof *tags, by_xref);
        for (size_t i = 0; i < ntags; i++)
            printf("%-16s %4d %-16s %s\n", tags[i].name, tags[i].line, tags[i].file, tags[i].text);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            fprintf(stderr, "ctags: standard output: %s\n", strerror(errno));
            return 1;
        }
        return status;
    }
    char **lines = xmalloc((ntags + 1) * sizeof *lines);
    size_t n = 0, cap = ntags + 1;
    bool to_stdout = strcmp(tagsfile, "-") == 0;
    if (o_a && !to_stdout) {
        FILE *f = fopen(tagsfile, "r");
        if (f) {
            char *line = NULL;
            size_t lcap = 0;
            ssize_t len;
            while ((len = getline(&line, &lcap, f)) >= 0) {
                if (len > 0 && line[len - 1] == '\n')
                    line[--len] = '\0';
                if (n == cap) {
                    cap *= 2;
                    lines = xrealloc(lines, cap * sizeof *lines);
                }
                lines[n++] = xstrdup(line);
            }
            free(line);
            fclose(f);
        } else if (errno != ENOENT) {
            fprintf(stderr, "ctags: %s: %s\n", tagsfile, strerror(errno));
            return 1;
        }
    }
    for (size_t i = 0; i < ntags; i++) {
        if (n == cap) {
            cap *= 2;
            lines = xrealloc(lines, cap * sizeof *lines);
        }
        lines[n++] = tag_line(&tags[i]);
    }
    qsort(lines, n, sizeof *lines, by_tagline);
    FILE *out = to_stdout ? stdout : fopen(tagsfile, "w");
    if (!out) {
        fprintf(stderr, "ctags: %s: %s\n", tagsfile, strerror(errno));
        return 1;
    }
    for (size_t i = 0; i < n; i++)
        if (i == 0 || strcmp(lines[i], lines[i - 1]) != 0)
            fprintf(out, "%s\n", lines[i]);
    if (fflush(out) != 0 || ferror(out) || (!to_stdout && fclose(out) != 0)) {
        fprintf(stderr, "ctags: %s: %s\n", to_stdout ? "standard output" : tagsfile, strerror(errno));
        return 1;
    }
    return status;
}
