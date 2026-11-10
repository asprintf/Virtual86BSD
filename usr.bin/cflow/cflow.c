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
 * cflow.c - cflow: generate a C-language flowgraph (POSIX.1-2024, XCU
 * cflow)
 *
 * Synopsis: cflow [-r] [-d num] [-D name[=def]]... [-i incl] [-I dir]...
 *                 [-U name]... file...
 *
 * The files, by their suffixes:
 *   .c (and any other suffix, unless the file is an ELF object):
 *       preprocessed (cpp.c, with -D, -I and -U in their order) and parsed
 *       (cparse.c);
 *   .i  the output of c17 -E: parsed (its line markers are honoured);
 *   .l  lex input: lex -t makes the C (shown as lex.yy.c, but for the
 *       lines of the .l file, which #line keeps);
 *   .y  yacc input: likewise with yacc -b (y.tab.c);
 *   .s  assembler: the labels (not those starting with ".") are the
 *       functions (data with ".type name, @object"), and call instructions
 *       (call, bl, jal, jsr, bsr ...) their calls;
 *   object files (ELF, relocatable): the defined function and data
 *       symbols, and the calls and references the relocations of the
 *       executable sections make (an undefined symbol is taken as a
 *       function; a call the assembler resolved, to a static function in
 *       the same section, leaves no relocation and is not seen).
 *
 * The graph: a function calls the functions it names (called or not),
 * and, with -i x, uses the data (objects) of file scope it names.  Each
 * line: "%d %s: %s\n" (the line number, the indentation: one space at
 * the top level, four per level below, the name, and its definition);
 * the definition is "type, <file line>" for C (the type abstract:
 * "int()", "char *"), "<file section>" from an object file, "<>" if
 * there is no definition, and only the number of the line where it was
 * written the second time a name is written (whose callees are then not
 * written again).  The callees of a function are in the order of their
 * names (LC_COLLATE).  The top level: main, then the other functions
 * that no other function calls, then any function not written yet (those
 * only called in a cycle), in the order of their names.
 *
 * -r: each name (in the order of the names) with its callers below it,
 * and theirs below them.
 * -d num: the lines of the levels from num (0: the top level) are not
 * written; a num that is not positive is ignored.
 * -i x: the data symbols too (external and static, of file scope);
 * -i _: the names that start with "_" too (left out otherwise).
 *
 * Choices where the standard leaves room:
 *  - Functions and objects are known by name, but those declared static
 *    (local symbols of an object file) are those of their file: two
 *    static functions of the same name in two files are two (written in
 *    the order of their files).  The definition written is the first.
 *  - A file that cannot be read (or made by lex or yacc): a diagnostic,
 *    status 1; the graph of the other files is written.  Problems in the
 *    C (an #include not found, an #error) are warnings.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L

#include "cparse.h"
#include "cpp.h"
#include "elf.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct sym {
    char *name;
    const char *unit;           /* static: the file it is in; NULL: external */
    bool isfunc, isdata, defined;
    char *def;
    struct sym **callees, **callers;
    size_t ncallees, ncallers, capcallees, capcallers;
    bool called;                /* by another function */
    long printed;               /* the line where it was written */
    bool reached;               /* in the graph written (any level) */
    struct sym *hnext;
};

#define NHASH 4099
static struct sym *table[NHASH];
static struct sym **all;
static size_t nall, capall;
static bool o_r, o_x, o_under;
static long depth = LONG_MAX;
static int status;

static unsigned hash(const char *s)
{
    unsigned h = 5381;
    for (; *s; s++)
        h = h * 33 + (unsigned char)*s;
    return h % NHASH;
}

/* the symbol of a name: external (unit NULL), or static in a file */
static struct sym *get_in(const char *name, const char *unit)
{
    unsigned h = hash(name);
    for (struct sym *s = table[h]; s; s = s->hnext)
        if (strcmp(s->name, name) == 0 && (s->unit == unit || (s->unit && unit && strcmp(s->unit, unit) == 0)))
            return s;
    struct sym *s = xcalloc(1, sizeof *s);
    s->name = xstrdup(name);
    s->unit = unit ? xstrdup(unit) : NULL;
    s->hnext = table[h];
    table[h] = s;
    if (nall == capall) {
        capall = capall ? 2 * capall : 256;
        all = xrealloc(all, capall * sizeof *all);
    }
    all[nall++] = s;
    return s;
}

static struct sym *get(const char *name)
{
    return get_in(name, NULL);
}

static void add_to(struct sym ***v, size_t *n, size_t *cap, struct sym *s)
{
    for (size_t i = 0; i < *n; i++)
        if ((*v)[i] == s)
            return;
    if (*n == *cap) {
        *cap = *cap ? 2 * *cap : 8;
        *v = xrealloc(*v, *cap * sizeof **v);
    }
    (*v)[(*n)++] = s;
}

static void edge(struct sym *from, struct sym *to)
{
    add_to(&from->callees, &from->ncallees, &from->capcallees, to);
    add_to(&to->callers, &to->ncallers, &to->capcallers, from);
    if (from != to)
        to->called = true;
}

static void define(struct sym *s, const char *def)
{
    if (s->defined)
        return;
    s->defined = true;
    s->def = xstrdup(def);
}

/* ---- C ---- */

/* the C made by lex or yacc: its name in a temporary directory, and the
   name it is shown as (also for "<stdout>", lex -t's name for it) */
static const char *gen_path, *gen_name;

static const char *shown(const char *file)
{
    if (gen_name && (strcmp(file, gen_path) == 0 || strcmp(file, "<stdout>") == 0))
        return gen_name;
    return file;
}

/* the declarations and references of a translation unit, resolved when
   it is all read: a name declared static in it is its own */
struct ev {
    enum { E_FUNCDEF, E_FUNC, E_VAR, E_CALL, E_DATA } what;
    const char *name, *func;
    char *def;                  /* E_FUNCDEF, E_VAR: the definition, or NULL */
};

static struct ev *evs;
static size_t nev, capev;
static struct name {
    const char *name;
    struct name *next;
} *statics[NHASH];

static void add_ev(int what, const char *name, const char *func, char *def)
{
    if (nev == capev) {
        capev = capev ? 2 * capev : 1024;
        evs = xrealloc(evs, capev * sizeof *evs);
    }
    evs[nev++] = (struct ev){ what, name, func, def };
}

static void add_static(const char *name)
{
    unsigned h = hash(name);
    for (struct name *n = statics[h]; n; n = n->next)
        if (strcmp(n->name, name) == 0)
            return;
    struct name *n = xmalloc(sizeof *n);
    n->name = name;
    n->next = statics[h];
    statics[h] = n;
}

static bool is_static(const char *name)
{
    for (struct name *n = statics[hash(name)]; n; n = n->next)
        if (strcmp(n->name, name) == 0)
            return true;
    return false;
}

static char *c_def(const struct cdecl *d)
{
    size_t n = strlen(d->type) + strlen(d->name->file) + 32;
    char *def = xmalloc(n);
    snprintf(def, n, "%s, <%s %d>", d->type, shown(d->name->file), d->name->oline);
    return def;
}

static void on_decl(void *arg, const struct cdecl *d)
{
    (void)arg;
    if (!d->file_scope)
        return;
    if (d->is_static && (d->kind == CD_FUNCDEF || d->kind == CD_FUNC || d->kind == CD_VAR))
        add_static(d->name->text);
    if (d->kind == CD_FUNCDEF)
        add_ev(E_FUNCDEF, d->name->text, NULL, c_def(d));
    else if (d->kind == CD_FUNC)
        add_ev(E_FUNC, d->name->text, NULL, NULL);
    else if (d->kind == CD_VAR)
        add_ev(E_VAR, d->name->text, NULL, !d->is_extern || d->has_init ? c_def(d) : NULL);
}

static void on_ref(void *arg, const struct cref *r)
{
    (void)arg;
    if (!r->func || r->local)
        return;
    if (r->kind == CD_ENUMCONST || r->kind == CD_TYPEDEF)
        return;
    if (r->call || r->kind == CD_FUNC || r->kind == CD_FUNCDEF)
        add_ev(E_CALL, r->name->text, r->func, NULL);
    else if (r->kind == CD_VAR)
        add_ev(E_DATA, r->name->text, r->func, NULL);
}

static void resolve(const char *unit)
{
    for (size_t i = 0; i < nev; i++) {
        struct ev *e = &evs[i];
        struct sym *s = get_in(e->name, is_static(e->name) ? unit : NULL);
        switch (e->what) {
        case E_FUNCDEF:
            s->isfunc = true;
            define(s, e->def);
            break;
        case E_FUNC:
            s->isfunc = true;
            break;
        case E_VAR:
            s->isdata = true;
            if (e->def)
                define(s, e->def);
            break;
        case E_CALL:
        case E_DATA: {
            struct sym *from = get_in(e->func, is_static(e->func) ? unit : NULL);
            if (e->what == E_DATA)
                s->isdata = true;
            else if (!s->isdata)
                s->isfunc = true;
            edge(from, s);
            break;
        }
        }
        free(e->def);
    }
    nev = 0;
    for (size_t i = 0; i < NHASH; i++)
        while (statics[i]) {
            struct name *n = statics[i];
            statics[i] = n->next;
            free(n);
        }
}

static struct cpp *cpp;

static bool c_file(const char *path, const char *display, const char *quotedir)
{
    cpp_set_quote_dir(cpp, quotedir);
    struct token *t = cpp_run(cpp, path, display);
    if (!t)
        return false;
    struct cparse_hooks h = { on_decl, on_ref, NULL };
    cparse(t, &h);
    resolve(display);
    return true;
}

/* ---- lex and yacc ---- */

static char *tmpdir;

static void rmtmp(void)
{
    if (!tmpdir)
        return;
    char *a = xmalloc(strlen(tmpdir) + 16);
    sprintf(a, "%s/lex.yy.c", tmpdir);
    unlink(a);
    sprintf(a, "%s/y.tab.c", tmpdir);
    unlink(a);
    sprintf(a, "%s/y.tab.h", tmpdir);
    unlink(a);
    free(a);
    rmdir(tmpdir);
}

static const char *get_tmpdir(void)
{
    if (tmpdir)
        return tmpdir;
    const char *t = getenv("TMPDIR");
    if (!t || !*t)
        t = "/tmp";
    tmpdir = xmalloc(strlen(t) + 16);
    sprintf(tmpdir, "%s/cflowXXXXXX", t);
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "cflow: %s: %s\n", tmpdir, strerror(errno));
        exit(1);
    }
    atexit(rmtmp);
    return tmpdir;
}

/* run argv with the standard output to `out` (NULL: as it is); true if
   it exits with status 0 */
static bool run(char *const argv[], const char *out)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "cflow: fork: %s\n", strerror(errno));
        return false;
    }
    if (pid == 0) {
        if (out) {
            int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd < 0 || dup2(fd, 1) < 0)
                _exit(126);
            close(fd);
        } else {
            dup2(2, 1);
        }
        execvp(argv[0], argv);
        fprintf(stderr, "cflow: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    int st;
    while (waitpid(pid, &st, 0) < 0)
        if (errno != EINTR)
            return false;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

static char *dir_of(const char *path)
{
    const char *s = strrchr(path, '/');
    if (!s)
        return xstrdup(".");
    return xstrndup(path, s == path ? 1 : (size_t)(s - path));
}

/* lex or yacc input: true if done; diagnostics written otherwise */
static bool generated(const char *path, bool yacc)
{
    if (access(path, R_OK) != 0) {
        fprintf(stderr, "cflow: %s: %s\n", path, strerror(errno));
        return false;
    }
    const char *d = get_tmpdir();
    char *out = xmalloc(strlen(d) + 16);
    bool ok;
    if (yacc) {
        char *prefix = xmalloc(strlen(d) + 4);
        sprintf(prefix, "%s/y", d);
        char *argv[] = { "yacc", "-b", prefix, (char *)path, NULL };
        ok = run(argv, NULL);
        sprintf(out, "%s/y.tab.c", d);
        free(prefix);
    } else {
        char *argv[] = { "lex", "-t", (char *)path, NULL };
        sprintf(out, "%s/lex.yy.c", d);
        ok = run(argv, out);
    }
    if (!ok) {
        fprintf(stderr, "cflow: %s: %s failed\n", path, yacc ? "yacc" : "lex");
    } else {
        char *qd = dir_of(path);
        gen_path = out;
        gen_name = yacc ? "y.tab.c" : "lex.yy.c";
        ok = c_file(out, gen_name, qd);
        gen_path = gen_name = NULL;
        if (!ok)
            fprintf(stderr, "cflow: %s: %s\n", out, strerror(errno));
        free(qd);
    }
    free(out);
    return ok;
}

/* ---- assembler ---- */

static bool asm_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    static const char *const calls[] = {
        "call", "callq", "calll", "bl", "blx", "jal", "jsr", "bsr", "bsrl", "brasl", "jsrl", NULL,
    };
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    long lineno = 0;
    struct sym *cur = NULL;
    while ((len = getline(&line, &cap, f)) >= 0) {
        lineno++;
        /* comments: # (at the start), ;, // */
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#')
            continue;
        for (char *q = p; *q; q++)
            if (*q == ';' || (*q == '/' && q[1] == '/') || *q == '\n') {
                *q = '\0';
                break;
            }
        /* .type name, @object (or @function) */
        if (strncmp(p, ".type", 5) == 0 && (p[5] == ' ' || p[5] == '\t')) {
            char *n = p + 5;
            while (*n == ' ' || *n == '\t')
                n++;
            char *e = n;
            while (isalnum((unsigned char)*e) || *e == '_' || *e == '.' || *e == '$')
                e++;
            if (e > n && *e == ',') {
                *e = '\0';
                bool obj = strstr(e + 1, "object") != NULL;
                struct sym *s = get(n);
                if (obj)
                    s->isdata = true;
                else
                    s->isfunc = true;
            }
            continue;
        }
        /* labels */
        for (;;) {
            char *q = p;
            while (isalnum((unsigned char)*q) || *q == '_' || *q == '.' || *q == '$')
                q++;
            if (q == p || *q != ':')
                break;
            if (p[0] != '.' && !isdigit((unsigned char)p[0])) {
                *q = '\0';
                struct sym *s = get(p);
                char def[64 + PATH_MAX];
                snprintf(def, sizeof def, "<%s %ld>", path, lineno);
                define(s, def);
                if (s->isdata) {
                    cur = NULL;
                } else {
                    s->isfunc = true;
                    cur = s;
                }
            }
            p = q + 1;
            while (*p == ' ' || *p == '\t')
                p++;
        }
        /* a call */
        char *m = p;
        while (isalnum((unsigned char)*p))
            p++;
        size_t ml = (size_t)(p - m);
        if (ml == 0 || !cur)
            continue;
        bool is_call = false;
        for (const char *const *c = calls; *c; c++)
            if (strlen(*c) == ml && strncmp(m, *c, ml) == 0)
                is_call = true;
        if (!is_call)
            continue;
        while (*p == ' ' || *p == '\t' || *p == '*')
            p++;
        char *n = p;
        while (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')
            p++;
        if (p == n || isdigit((unsigned char)*n) || *n == '.')
            continue;
        *p = '\0';
        struct sym *to = get(n);
        to->isfunc = true;
        edge(cur, to);
    }
    free(line);
    fclose(f);
    return true;
}

/* ---- object files ---- */

static const char *location(const struct elf *e, size_t shndx)
{
    static char buf[64];
    if (shndx >= e->nsec)
        return "abs";
    const char *n = e->sec[shndx].name;
    if (*n == '.')
        n++;
    size_t l = strcspn(n, ".");
    if (l >= sizeof buf)
        l = sizeof buf - 1;
    memcpy(buf, n, l);
    buf[l] = '\0';
    return buf;
}

static void object(const char *path, const unsigned char *data, size_t size)
{
    struct elf e;
    const char *why;
    if (!elf_open(&e, data, size, &why)) {
        fprintf(stderr, "cflow: %s: %s\n", path, why);
        status = 1;
        return;
    }
    size_t st = elf_find_symtab(&e, SHT_SYMTAB);
    if (!st) {
        elf_close(&e);
        return;
    }
    size_t n = elf_nsyms(&e, st);
    struct elf_symbol *syms = xcalloc(n, sizeof *syms);
    for (size_t i = 1; i < n; i++) {
        if (!elf_symbol(&e, st, i, &syms[i]))
            continue;
        struct elf_symbol *y = &syms[i];
        if (!y->name[0] || y->shndx == SHN_UNDEF || y->shndx >= e.nsec)
            continue;
        if (y->type != STT_FUNC && y->type != STT_OBJECT && y->type != STT_GNU_IFUNC)
            continue;
        struct sym *s = get_in(y->name, y->bind == STB_LOCAL ? path : NULL);
        if (y->type == STT_OBJECT)
            s->isdata = true;
        else
            s->isfunc = true;
        char def[64 + PATH_MAX];
        snprintf(def, sizeof def, "<%s %s>", path, location(&e, y->shndx));
        define(s, def);
    }
    for (size_t r = 0; r < e.nsec; r++) {
        const struct elf_section *rs = &e.sec[r];
        if ((rs->type != SHT_RELA && rs->type != SHT_REL) || rs->link != st || rs->info >= e.nsec)
            continue;
        size_t target = rs->info;
        if (!(e.sec[target].flags & SHF_EXECINSTR))
            continue;
        bool rela = rs->type == SHT_RELA;
        size_t esz = rs->entsize ? rs->entsize : (e.is64 ? (rela ? 24 : 16) : (rela ? 12 : 8));
        if (rs->offset > e.size || rs->size > e.size - rs->offset)
            continue;
        int w = e.is64 ? 8 : 4;
        for (size_t k = 0; k + esz <= rs->size; k += esz) {
            const unsigned char *p = e.data + rs->offset + k;
            uint64_t off = elf_get(&e, p, w), info = elf_get(&e, p + w, w);
            int64_t addend = rela ? (int64_t)elf_get(&e, p + 2 * w, w) : 0;
            if (rela && !e.is64)
                addend = (int32_t)addend;
            size_t si = e.is64 ? (size_t)(info >> 32) : (size_t)(info >> 8);
            if (si == 0 || si >= n)
                continue;
            /* the function the relocation is in */
            struct elf_symbol *in = NULL;
            for (size_t i = 1; i < n; i++) {
                struct elf_symbol *y = &syms[i];
                if (y->shndx != target || (y->type != STT_FUNC && y->type != STT_GNU_IFUNC) || !y->name ||
                    !y->name[0])
                    continue;
                if (y->value <= off && (off < y->value + y->size || y->size == 0) &&
                    (!in || y->value > in->value))
                    in = y;
            }
            if (!in)
                continue;
            struct elf_symbol *to = &syms[si];
            if (to->type == STT_SECTION) {
                /* section + addend: the symbol there */
                struct elf_symbol *found = NULL;
                for (size_t i = 1; i < n && rela; i++) {
                    struct elf_symbol *y = &syms[i];
                    if (y->shndx == to->shndx && y->name && y->name[0] &&
                        (y->type == STT_FUNC || y->type == STT_OBJECT) &&
                        ((int64_t)y->value == addend || (int64_t)y->value == addend + 4))
                        found = y;
                }
                if (!found)
                    continue;
                to = found;
            }
            if (!to->name || !to->name[0] || to->type == STT_FILE || to->type == STT_SECTION)
                continue;
            struct sym *from = get_in(in->name, in->bind == STB_LOCAL ? path : NULL);
            struct sym *callee = get_in(to->name, to->bind == STB_LOCAL ? path : NULL);
            if (to->type == STT_OBJECT)
                callee->isdata = true;
            else if (!callee->isdata)
                callee->isfunc = true;
            edge(from, callee);
        }
    }
    free(syms);
    elf_close(&e);
}

static bool object_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    size_t cap = 65536, n = 0, r;
    unsigned char *d = xmalloc(cap);
    while ((r = fread(d + n, 1, cap - n, f)) > 0) {
        n += r;
        if (n == cap) {
            cap *= 2;
            d = xrealloc(d, cap);
        }
    }
    bool err = ferror(f);
    fclose(f);
    if (err) {
        free(d);
        return false;
    }
    object(path, d, n);
    free(d);
    return true;
}

static bool is_elf_file(const char *path)
{
    unsigned char m[4];
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    bool r = fread(m, 1, 4, f) == 4 && memcmp(m, "\177ELF", 4) == 0;
    fclose(f);
    return r;
}

/* ---- output ---- */

static long lineno;

static bool visible(const struct sym *s)
{
    if (s->name[0] == '_' && !o_under)
        return false;
    if (s->isfunc)
        return true;
    return s->isdata && o_x;
}

static int by_name(const void *a, const void *b)
{
    const struct sym *x = *(struct sym *const *)a, *y = *(struct sym *const *)b;
    int c = strcoll(x->name, y->name);
    if (c == 0)
        c = strcmp(x->name, y->name);
    if (c == 0 && x->unit != y->unit)
        c = !x->unit ? -1 : !y->unit ? 1 : strcmp(x->unit, y->unit);
    return c;
}

/* mark what the graph from s has, whatever the depth */
static void reach(struct sym *s)
{
    if (s->reached)
        return;
    s->reached = true;
    for (size_t i = 0; i < s->ncallees; i++)
        if (visible(s->callees[i]))
            reach(s->callees[i]);
}

static void print(struct sym *s, long level)
{
    if (level >= depth)
        return;
    lineno++;
    printf("%ld", lineno);
    if (level == 0)
        putchar(' ');
    for (long i = 0; i < 4 * level; i++)
        putchar(' ');
    if (s->printed) {
        printf("%s: %ld\n", s->name, s->printed);
        return;
    }
    s->printed = lineno;
    printf("%s: %s\n", s->name, s->defined ? s->def : "<>");
    struct sym **v = o_r ? s->callers : s->callees;
    size_t n = o_r ? s->ncallers : s->ncallees;
    struct sym **w = xmalloc((n ? n : 1) * sizeof *w);
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (visible(v[i]))
            w[k++] = v[i];
    qsort(w, k, sizeof *w, by_name);
    for (size_t i = 0; i < k; i++)
        print(w[i], level + 1);
    free(w);
}

static void usage(void)
{
    fputs("usage: cflow [-r] [-d num] [-D name[=def]]... [-i incl] [-I dir]... [-U name]... file...\n",
          stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "cflow";
    cpp = cpp_new();
    int c;
    while ((c = getopt(argc, argv, "rd:i:D:I:U:")) != -1) {
        switch (c) {
        case 'r': o_r = true; break;
        case 'd': {
            char *end;
            errno = 0;
            long v = strtol(optarg, &end, 10);
            if (end == optarg || *end || errno)
                usage();
            if (v > 0)
                depth = v;
            break;
        }
        case 'i':
            for (const char *p = optarg; *p; p++) {
                if (*p == 'x')
                    o_x = true;
                else if (*p == '_')
                    o_under = true;
                else
                    usage();
            }
            break;
        case 'D': cpp_define(cpp, optarg); break;
        case 'U': cpp_undef(cpp, optarg); break;
        case 'I': cpp_include_dir(cpp, optarg); break;
        default: usage();
        }
    }
    if (optind >= argc)
        usage();
    for (int i = optind; i < argc; i++) {
        const char *path = argv[i];
        size_t l = strlen(path);
        const char *suf = l >= 2 && path[l - 2] == '.' ? path + l - 1 : "";
        bool ok;
        if (strcmp(suf, "y") == 0 || strcmp(suf, "l") == 0) {
            if (!generated(path, suf[0] == 'y'))
                status = 1;
            continue;
        } else if (strcmp(suf, "s") == 0 || strcmp(suf, "S") == 0) {
            ok = asm_file(path);
        } else if (is_elf_file(path)) {
            ok = object_file(path);
        } else {
            ok = c_file(path, path, NULL);
        }
        if (!ok) {
            fprintf(stderr, "cflow: %s: %s\n", path, strerror(errno));
            status = 1;
        }
    }
    struct sym **v = xmalloc((nall ? nall : 1) * sizeof *v);
    if (nall)
        memcpy(v, all, nall * sizeof *v);
    qsort(v, nall, sizeof *v, by_name);
    if (o_r) {
        for (size_t i = 0; i < nall; i++)
            if (visible(v[i]) && (v[i]->defined || v[i]->ncallers > 0))
                print(v[i], 0);
    } else {
        struct sym *m = NULL;
        for (size_t i = 0; i < nall; i++)
            if (strcmp(v[i]->name, "main") == 0 && v[i]->defined && v[i]->isfunc && !v[i]->called)
                m = v[i];
        if (m && visible(m)) {
            print(m, 0);
            reach(m);
        }
        for (size_t i = 0; i < nall; i++)
            if (v[i] != m && v[i]->defined && v[i]->isfunc && !v[i]->called && !v[i]->reached &&
                visible(v[i])) {
                print(v[i], 0);
                reach(v[i]);
            }
        for (size_t i = 0; i < nall; i++)
            if (v[i]->defined && v[i]->isfunc && !v[i]->reached && visible(v[i])) {
                print(v[i], 0);
                reach(v[i]);
            }
    }
    free(v);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "cflow: standard output: %s\n", strerror(errno));
        return 1;
    }
    return status;
}
