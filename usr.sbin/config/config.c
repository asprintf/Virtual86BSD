/*
 * Copyright (c) 2026, asprintf
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
 * config.c - config: build a kernel configuration (BSD config(8))
 *
 * Synopsis: config [-s srcdir] [-b builddir] configfile
 *
 * Reads the machine-independent sources list (srcdir/conf/files), the
 * machine's (srcdir/arch/MACHINE/conf/files.MACHINE) and the configuration
 * file (GENERIC), and writes, into builddir (default ../compile/NAME), what
 * the kernel is built from:
 *
 *   Makefile    the template srcdir/arch/MACHINE/conf/Makefile.MACHINE with the
 *               object files and their rules filled in
 *   ioconf.c    the table of devices: which drivers, attached where
 *   locators.h  the numbers of the locators (ISACF_PORT ...)
 *   options.h   the options as #defines
 *   machine     a link to srcdir/arch/MACHINE/include, for <machine/...>
 *
 * It is meant to be run in srcdir/arch/MACHINE/conf, where the defaults
 * (srcdir ../../.., builddir ../compile/NAME) are right.  What it writes is
 * a function of the three input files alone (no date, no host, no absolute
 * path), and a file that would not change is left alone, so make does not
 * build again what did not change.  The syntax is in README.md.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------ utilities */

static const char *cur_file; /* for messages */
static int cur_line;
static int nerrors;

static void err(const char *fmt, ...)
{
    va_list ap;
    if (cur_file)
        fprintf(stderr, "config: %s:%d: ", cur_file, cur_line);
    else
        fprintf(stderr, "config: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    nerrors++;
}

static void die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "config: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

static char *xstrdup(const char *s)
{
    char *p = xmalloc(strlen(s) + 1);
    return strcpy(p, s);
}

/* a growing string */
struct buf {
    char *s;
    size_t len, cap;
};

static void bputs(struct buf *b, const char *s)
{
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->s = xrealloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, s, n + 1);
    b->len += n;
}

static void bprintf(struct buf *b, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof tmp)
        die("line too long");
    bputs(b, tmp);
}

/* a list of strings */
struct list {
    char **v;
    int n;
};

static void ladd(struct list *l, const char *s)
{
    l->v = xrealloc(l->v, (size_t)(l->n + 1) * sizeof *l->v);
    l->v[l->n++] = xstrdup(s);
}

static int lhas(const struct list *l, const char *s)
{
    for (int i = 0; i < l->n; i++)
        if (!strcmp(l->v[i], s))
            return 1;
    return 0;
}

/* the words of a line (blanks and commas separate them; with punct, `{', `}',
   `=', `|', `&', `!', `(' and `)' are words of their own).  The words are
   copies. */
static int words(const char *line, char **w, int max, int punct)
{
    int n = 0;
    const char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (!*p)
            break;
        if (n == max) {
            err("too many words");
            return n;
        }
        const char *start = p;
        if (punct && strchr("{}=|&!()", *p))
            p++;
        else
            while (*p && *p != ' ' && *p != '\t' && *p != ',' && !(punct && strchr("{}=|&!()", *p)))
                p++;
        char *t = xmalloc((size_t)(p - start) + 1);
        memcpy(t, start, (size_t)(p - start));
        t[p - start] = '\0';
        w[n++] = t;
    }
    return n;
}

/* ------------------------------------------------------------ the model */

struct locator {
    char *name, *def;
};

struct device {
    char *name;
    struct locator *locs;
    int nlocs;
    struct list parents; /* "attach NAME at ..." */
    int has_attach;
    int ninst;           /* instances in the configuration */
};

struct inst {
    struct device *dev;
    int unit;            /* -1: the star (any number) */
    struct device *pdev; /* NULL: the root */
    int punit;           /* -1: any */
    char **locv;         /* for each locator of pdev: a value or NULL */
    int line;
};

struct fentry {
    char *path;
    char *cond; /* NULL: always */
    const char *file;
    int line;
};

struct option {
    char *name;
    int set;
    char *value; /* NULL: 1 */
};

static struct device **devs;
static int ndevs;
static struct inst *insts;
static int ninsts;
static struct fentry *fentries;
static int nfentries;
static struct option *opts;
static int nopts;
static char *machine, *confname, *rootdev;
static int machine_seen;

static struct device *finddev(const char *name)
{
    for (int i = 0; i < ndevs; i++)
        if (!strcmp(devs[i]->name, name))
            return devs[i];
    return NULL;
}

static struct option *findopt(const char *name)
{
    for (int i = 0; i < nopts; i++)
        if (!strcmp(opts[i].name, name))
            return &opts[i];
    return NULL;
}

static int isname(const char *s)
{
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_'))
        return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_'))
            return 0;
    return 1;
}

/* ------------------------------------------------------------ reading */

/* the lines of a file, each cleaned of its comment and the blanks around;
   fn(line) is called for the ones that are not empty */
static void readlines(const char *path, void (*fn)(char *line))
{
    FILE *f = fopen(path, "r");
    if (!f)
        die("%s: %s", path, strerror(errno));
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    const char *save_file = cur_file;
    int save_line = cur_line;
    cur_file = path;
    cur_line = 0;
    while ((n = getline(&line, &cap, f)) != -1) {
        cur_line++;
        char *h = strchr(line, '#');
        if (h)
            *h = '\0';
        n = (ssize_t)strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
            line[--n] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p)
            fn(p);
    }
    free(line);
    fclose(f);
    cur_file = save_file;
    cur_line = save_line;
}

/* device NAME [{ loc = default, ... }] */
static void do_device(char **w, int n)
{
    if (n < 2 || !isname(w[1])) {
        err("device: a name is needed");
        return;
    }
    if (finddev(w[1])) {
        err("device %s is declared twice", w[1]);
        return;
    }
    struct device *d = xmalloc(sizeof *d);
    memset(d, 0, sizeof *d);
    d->name = xstrdup(w[1]);
    int i = 2;
    if (i < n) {
        if (strcmp(w[i], "{")) {
            err("device %s: `{' or the end of the line expected, not `%s'", d->name, w[i]);
            return;
        }
        i++;
        while (i < n && strcmp(w[i], "}")) {
            if (!isname(w[i])) {
                err("device %s: `%s' is not the name of a locator", d->name, w[i]);
                return;
            }
            struct locator l;
            l.name = xstrdup(w[i++]);
            l.def = xstrdup("-1");
            if (i < n && !strcmp(w[i], "=")) {
                if (i + 1 >= n || !strcmp(w[i + 1], "}")) {
                    err("device %s: a default for %s is missing", d->name, l.name);
                    return;
                }
                free(l.def);
                l.def = xstrdup(w[i + 1]);
                i += 2;
            }
            d->locs = xrealloc(d->locs, (size_t)(d->nlocs + 1) * sizeof *d->locs);
            d->locs[d->nlocs++] = l;
        }
        if (i >= n) {
            err("device %s: `}' is missing", d->name);
            return;
        }
        if (i + 1 < n) {
            err("device %s: `%s' after `}'", d->name, w[i + 1]);
            return;
        }
    }
    devs = xrealloc(devs, (size_t)(ndevs + 1) * sizeof *devs);
    devs[ndevs++] = d;
}

/* attach NAME at PARENT [PARENT ...] */
static void do_attach(char **w, int n)
{
    struct device *d;
    if (n < 4 || strcmp(w[2], "at")) {
        err("attach: attach NAME at PARENT [PARENT...]");
        return;
    }
    d = finddev(w[1]);
    if (!d) {
        err("attach: %s is not a device (declare it with `device' first)", w[1]);
        return;
    }
    if (d->has_attach) {
        err("%s is attached twice (one `attach' line, with all the parents)", d->name);
        return;
    }
    d->has_attach = 1;
    for (int i = 3; i < n; i++) {
        if (!isname(w[i])) {
            err("attach: `%s' is not a name", w[i]);
            return;
        }
        ladd(&d->parents, w[i]);
    }
}

/* defopt NAME... */
static void do_defopt(char **w, int n)
{
    if (n < 2) {
        err("defopt: a name is needed");
        return;
    }
    for (int i = 1; i < n; i++) {
        if (!isname(w[i])) {
            err("defopt: `%s' is not a name", w[i]);
            return;
        }
        if (findopt(w[i])) {
            err("option %s is declared twice", w[i]);
            return;
        }
        opts = xrealloc(opts, (size_t)(nopts + 1) * sizeof *opts);
        memset(&opts[nopts], 0, sizeof *opts);
        opts[nopts++].name = xstrdup(w[i]);
    }
}

/* file PATH [CONDITION] */
static void do_file(char *line)
{
    char *p = line + 4; /* after "file" */
    while (*p == ' ' || *p == '\t')
        p++;
    char *path = p;
    while (*p && *p != ' ' && *p != '\t')
        p++;
    if (*p)
        *p++ = '\0';
    while (*p == ' ' || *p == '\t')
        p++;
    size_t pl = strlen(path);
    if (!pl || pl < 3 || (strcmp(path + pl - 2, ".c") && strcmp(path + pl - 2, ".s"))) {
        err("file: `%s' is not a .c or .s file", path);
        return;
    }
    struct fentry e;
    e.path = xstrdup(path);
    e.cond = *p ? xstrdup(p) : NULL;
    e.file = xstrdup(cur_file);
    e.line = cur_line;
    fentries = xrealloc(fentries, (size_t)(nfentries + 1) * sizeof *fentries);
    fentries[nfentries++] = e;
}

static void files_line(char *line)
{
    char *w[64];
    if (!strncmp(line, "file", 4) && (line[4] == ' ' || line[4] == '\t')) {
        do_file(line);
        return;
    }
    int n = words(line, w, 64, 1);
    if (!strcmp(w[0], "device"))
        do_device(w, n);
    else if (!strcmp(w[0], "attach"))
        do_attach(w, n);
    else if (!strcmp(w[0], "defopt"))
        do_defopt(w, n);
    else
        err("unknown keyword `%s'", w[0]);
}

/* ------------------------------------------------------------ the configuration file */

/* "name" and its trailing unit: com0 -> com, 0; pci* -> pci, -1 (star); the
   parent of "pci?" -> pci, -2 (any) */
static int splitunit(char *s, int parent, int *unit)
{
    size_t n = strlen(s);
    if (n && s[n - 1] == '*') {
        s[n - 1] = '\0';
        *unit = -1;
        return 1;
    }
    if (n && s[n - 1] == '?') {
        if (!parent)
            return 0;
        s[n - 1] = '\0';
        *unit = -1;
        return 1;
    }
    size_t i = n;
    while (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9')
        i--;
    if (i == n || i == 0)
        return 0;
    if (n - i > 4)
        return 0;
    *unit = atoi(s + i);
    s[i] = '\0';
    return 1;
}

static int parse_number(const char *s, long *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (errno || end == s || *end || v < -1 || v > 65535)
        return 0;
    *out = v;
    return 1;
}

static void conf_line(char *line)
{
    char *w[64];
    int n = words(line, w, 64, 0);
    if (!strcmp(w[0], "machine")) {
        if (n != 2 || !isname(w[1])) {
            err("machine NAME");
            return;
        }
        if (machine_seen) {
            err("machine is given twice");
            return;
        }
        machine_seen = 1;
    } else if (!strcmp(w[0], "option")) {
        /* option NAME  or  option NAME=VALUE (the words are NAME=VALUE
           when written without blanks) */
        if (n != 2 && n != 4) {
            err("option NAME  or  option NAME=VALUE");
            return;
        }
        char *name = w[1], *val = NULL;
        char *eq = strchr(name, '=');
        if (n == 2 && eq) {
            *eq = '\0';
            val = eq + 1;
        } else if (n == 4 && !strcmp(w[2], "=")) {
            val = w[3];
        } else if (n == 4) {
            err("option NAME  or  option NAME=VALUE");
            return;
        }
        struct option *o = findopt(name);
        if (!o) {
            err("unknown option %s (it is not declared by a defopt line in the files)", name);
            return;
        }
        if (o->set) {
            err("option %s is given twice", name);
            return;
        }
        if (val && !*val) {
            err("option %s: the value is empty", name);
            return;
        }
        o->set = 1;
        o->value = val ? xstrdup(val) : NULL;
    } else if (!strcmp(w[0], "config")) {
        /* config NAME [root on DEVICE] */
        if (!(n == 2 || (n == 5 && !strcmp(w[2], "root") && !strcmp(w[3], "on")))) {
            err("config NAME [root on DEVICE]");
            return;
        }
        if (strcmp(w[1], "bsd")) {
            err("config: the kernel is named bsd, not %s", w[1]);
            return;
        }
        if (rootdev) {
            err("config is given twice");
            return;
        }
        rootdev = xstrdup(n == 5 ? w[4] : "");
    } else if (n >= 3 && !strcmp(w[1], "at")) {
        /* NAME[N|*] at PARENT[N|*|?|root] [locator value]... */
        struct inst in;
        memset(&in, 0, sizeof in);
        in.line = cur_line;
        char *name = w[0], *parent = w[2];
        if (!splitunit(name, 0, &in.unit) || !isname(name)) {
            err("`%s': a device is written with its unit (com0) or a star (pci*)", w[0]);
            return;
        }
        in.dev = finddev(name);
        if (!in.dev) {
            err("no device %s", name);
            return;
        }
        if (!strcmp(parent, "root")) {
            in.pdev = NULL;
            in.punit = -1;
        } else {
            if (!splitunit(parent, 1, &in.punit) || !isname(parent)) {
                err("`%s': a parent is written with its unit (pci0) or `?' (pci?)", w[2]);
                return;
            }
            in.pdev = finddev(parent);
            if (!in.pdev) {
                err("no device %s", parent);
                return;
            }
        }
        if (!in.dev->has_attach) {
            err("device %s has no attach line", in.dev->name);
            return;
        }
        if (!lhas(&in.dev->parents, in.pdev ? in.pdev->name : "root")) {
            err("%s does not attach at %s", in.dev->name, in.pdev ? in.pdev->name : "root");
            return;
        }
        int nl = in.pdev ? in.pdev->nlocs : 0;
        in.locv = xmalloc((size_t)(nl ? nl : 1) * sizeof *in.locv);
        for (int i = 0; i < nl; i++)
            in.locv[i] = NULL;
        for (int i = 3; i < n; i += 2) {
            int k;
            for (k = 0; k < nl; k++)
                if (!strcmp(in.pdev->locs[k].name, w[i]))
                    break;
            if (k == nl) {
                err("%s has no locator %s", in.pdev ? in.pdev->name : "root", w[i]);
                return;
            }
            long v;
            if (i + 1 >= n || !parse_number(w[i + 1], &v)) {
                err("locator %s: a number is needed", w[i]);
                return;
            }
            in.locv[k] = w[i + 1];
            in.locv[k] = xstrdup(in.locv[k]);
        }
        for (int i = 0; i < ninsts; i++)
            if (insts[i].dev == in.dev && insts[i].unit == in.unit) {
                if (in.unit < 0)
                    break; /* more than one star line is all right: more parents */
                err("%s%d is there twice (line %d)", in.dev->name, in.unit, insts[i].line);
                return;
            }
        insts = xrealloc(insts, (size_t)(ninsts + 1) * sizeof *insts);
        insts[ninsts++] = in;
        in.dev->ninst++;
    } else {
        err("unknown line `%s'", w[0]);
    }
}

/* machine NAME comes first in the file, found before the files are read:
   the files of the machine are named after it */
static void find_machine(char *line)
{
    char *w[64];
    if (machine)
        return;
    int n = words(line, w, 64, 0);
    if (n == 2 && !strcmp(w[0], "machine") && isname(w[1]))
        machine = xstrdup(w[1]);
}

/* ------------------------------------------------------------ the conditions of file lines */

static const char *cp; /* what is left of the condition */
static int cond_bad;

static int cond_name_true(const char *name)
{
    struct device *d = finddev(name);
    if (d)
        return d->ninst > 0;
    struct option *o = findopt(name);
    if (o)
        return o->set;
    cond_bad = 1;
    err("the condition names `%s', which is not a device or an option", name);
    return 0;
}

static int cond_or(void);

static void cskip(void)
{
    while (*cp == ' ' || *cp == '\t')
        cp++;
}

static int cond_factor(void)
{
    cskip();
    if (*cp == '!') {
        cp++;
        return !cond_factor();
    }
    if (*cp == '(') {
        cp++;
        int v = cond_or();
        cskip();
        if (*cp != ')') {
            cond_bad = 1;
            err("the condition lacks a `)'");
            return 0;
        }
        cp++;
        return v;
    }
    char name[64];
    size_t n = 0;
    while ((*cp >= 'a' && *cp <= 'z') || (*cp >= 'A' && *cp <= 'Z') || (*cp >= '0' && *cp <= '9') || *cp == '_') {
        if (n < sizeof name - 1)
            name[n++] = *cp;
        cp++;
    }
    name[n] = '\0';
    if (!n) {
        cond_bad = 1;
        err("a name is expected in the condition, at `%s'", cp);
        return 0;
    }
    return cond_name_true(name);
}

static int cond_and(void)
{
    int v = cond_factor();
    for (;;) {
        cskip();
        if (*cp != '&')
            return v;
        cp++;
        int r = cond_factor();
        v = v && r;
    }
}

static int cond_or(void)
{
    int v = cond_and();
    for (;;) {
        cskip();
        if (*cp != '|')
            return v;
        cp++;
        int r = cond_and();
        v = v || r;
    }
}

static int cond_true(const struct fentry *e)
{
    if (!e->cond)
        return 1;
    cur_file = e->file;
    cur_line = e->line;
    cp = e->cond;
    cond_bad = 0;
    int v = cond_or();
    cskip();
    if (*cp && !cond_bad) {
        err("`%s' after the condition", cp);
    }
    cur_file = NULL;
    return v;
}

/* ------------------------------------------------------------ checks */

static void check(void)
{
    cur_file = confname;
    for (int i = 0; i < ninsts; i++) {
        struct inst *in = &insts[i];
        cur_line = in->line;
        if (!in->pdev)
            continue;
        /* the parent must be there: that unit, or any */
        int found = 0;
        for (int k = 0; k < ninsts; k++)
            if (insts[k].dev == in->pdev && (in->punit < 0 || insts[k].unit == in->punit || insts[k].unit < 0))
                found = 1;
        if (!found) {
            if (in->punit < 0)
                err("%s%s at %s?: no %s in the configuration", in->dev->name, in->unit < 0 ? "*" : "", in->pdev->name,
                    in->pdev->name);
            else
                err("%s at %s%d: there is no %s%d", in->dev->name, in->pdev->name, in->punit, in->pdev->name,
                    in->punit);
        }
    }
    cur_file = NULL;
}

/* ------------------------------------------------------------ output */

/* the path of TO seen from FROM: both are made real, and the route up from
   one and down to the other is written (../../..) */
static char *relpath(const char *from, const char *to)
{
    char a[PATH_MAX], b[PATH_MAX];
    if (!realpath(from, a) || !realpath(to, b))
        die("realpath: %s", strerror(errno));
    /* the components in common */
    const char *pa = a, *pb = b;
    for (;;) {
        while (*pa == '/')
            pa++;
        while (*pb == '/')
            pb++;
        const char *ea = pa, *eb = pb;
        while (*ea && *ea != '/')
            ea++;
        while (*eb && *eb != '/')
            eb++;
        if (ea == pa || eb == pb || ea - pa != eb - pb || memcmp(pa, pb, (size_t)(ea - pa)))
            break;
        pa = ea;
        pb = eb;
    }
    struct buf r = {0};
    for (;;) {
        while (*pa == '/')
            pa++;
        if (!*pa)
            break;
        while (*pa && *pa != '/')
            pa++;
        bputs(&r, r.len ? "/.." : "..");
    }
    while (*pb == '/')
        pb++;
    if (*pb) {
        if (r.len)
            bputs(&r, "/");
        bputs(&r, pb);
    }
    if (!r.len)
        bputs(&r, ".");
    return r.s;
}

static void mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    size_t n = strlen(path);
    if (n >= sizeof tmp)
        die("%s: name too long", path);
    memcpy(tmp, path, n + 1);
    for (size_t i = 1; i <= n; i++) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            char c = tmp[i];
            tmp[i] = '\0';
            if (mkdir(tmp, 0777) && errno != EEXIST)
                die("%s: %s", tmp, strerror(errno));
            tmp[i] = c;
        }
    }
}

/* the file DIR/NAME with the contents of B, unless it has them already */
static void put(const char *dir, const char *name, const struct buf *b)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (f) {
        char *old = xmalloc(b->len + 1);
        size_t n = fread(old, 1, b->len + 1, f);
        int same = n == b->len && !memcmp(old, b->s, b->len);
        free(old);
        fclose(f);
        if (same)
            return;
    }
    f = fopen(path, "wb");
    if (!f)
        die("%s: %s", path, strerror(errno));
    if (fwrite(b->s, 1, b->len, f) != b->len || fclose(f))
        die("%s: %s", path, strerror(errno));
}

static void upper(char *dst, const char *src)
{
    for (; *src; src++)
        *dst++ = (*src >= 'a' && *src <= 'z') ? (char)(*src - 32) : *src;
    *dst = '\0';
}

static void write_options(const char *dir)
{
    struct buf b = {0};
    bprintf(&b, "/* options.h: made by config(8) from %s; not to be edited */\n", confname);
    bputs(&b, "#ifndef _OPTIONS_H_\n#define _OPTIONS_H_\n\n");
    for (int i = 0; i < nopts; i++)
        if (opts[i].set)
            bprintf(&b, "#define %s %s\n", opts[i].name, opts[i].value ? opts[i].value : "1");
    bputs(&b, "\n#endif\n");
    put(dir, "options.h", &b);
    free(b.s);
}

static void write_locators(const char *dir)
{
    struct buf b = {0};
    bprintf(&b, "/* locators.h: made by config(8) from %s; not to be edited */\n", confname);
    bputs(&b, "#ifndef _LOCATORS_H_\n#define _LOCATORS_H_\n");
    for (int i = 0; i < ndevs; i++) {
        struct device *d = devs[i];
        char up[64], lup[64];
        if (!d->nlocs)
            continue;
        upper(up, d->name);
        bputs(&b, "\n");
        for (int k = 0; k < d->nlocs; k++) {
            upper(lup, d->locs[k].name);
            bprintf(&b, "#define %sCF_%s %d\n", up, lup, k);
            bprintf(&b, "#define %sCF_%s_DEFAULT %s\n", up, lup, d->locs[k].def);
        }
        bprintf(&b, "#define %sCF_NLOC %d\n", up, d->nlocs);
    }
    bputs(&b, "\n#endif\n");
    put(dir, "locators.h", &b);
    free(b.s);
}

static void write_ioconf(const char *dir)
{
    struct buf b = {0};
    bprintf(&b, "/* ioconf.c: made by config(8) from %s; not to be edited */\n", confname);
    bputs(&b, "#include <sys/param.h>\n#include <sys/device.h>\n\n");
    /* the drivers in use, in the order of the configuration */
    struct list used = {0};
    for (int i = 0; i < ninsts; i++)
        if (!lhas(&used, insts[i].dev->name))
            ladd(&used, insts[i].dev->name);
    for (int i = 0; i < used.n; i++)
        bprintf(&b, "extern struct cfdriver %s_cd;\nextern struct cfattach %s_ca;\n", used.v[i], used.v[i]);
    bputs(&b, "\n");
    for (int i = 0; i < ninsts; i++) {
        struct inst *in = &insts[i];
        if (!in->pdev || !in->pdev->nlocs)
            continue;
        bprintf(&b, "static int loc%d[] = {", i);
        for (int k = 0; k < in->pdev->nlocs; k++)
            bprintf(&b, "%s%s", k ? ", " : "", in->locv[k] ? in->locv[k] : in->pdev->locs[k].def);
        bputs(&b, "};\n");
    }
    bputs(&b, "\n/* attach, driver, unit, state, locators, flags, parent driver, parent unit, next star unit */\n");
    bputs(&b, "struct cfdata cfdata[] = {\n");
    for (int i = 0; i < ninsts; i++) {
        struct inst *in = &insts[i];
        bprintf(&b, "    {&%s_ca, &%s_cd, %d, %s, ", in->dev->name, in->dev->name, in->unit < 0 ? 0 : in->unit,
                in->unit < 0 ? "FSTATE_STAR" : "FSTATE_NORMAL");
        if (in->pdev && in->pdev->nlocs)
            bprintf(&b, "loc%d", i);
        else
            bputs(&b, "NULL");
        if (in->pdev)
            bprintf(&b, ", 0, &%s_cd, %d, 0},\n", in->pdev->name, in->punit);
        else
            bputs(&b, ", 0, NULL, -1, 0},\n");
    }
    bputs(&b, "    {NULL}\n};\n");
    bprintf(&b, "\nconst char cf_rootdev[] = \"%s\";\n", rootdev ? rootdev : "");
    put(dir, "ioconf.c", &b);
    free(b.s);
}

static int cmpstr(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* the .h files under DIR, as paths from the sources (S/...), skipping the
   compile directories, the other machines and the stand directories */
static void headers(const char *srcdir, const char *rel, struct list *out)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", srcdir, rel);
    DIR *d = opendir(path);
    if (!d)
        return;
    struct list names = {0};
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.')
            ladd(&names, e->d_name);
    closedir(d);
    qsort(names.v, (size_t)names.n, sizeof *names.v, cmpstr);
    for (int i = 0; i < names.n; i++) {
        char sub[PATH_MAX], full[PATH_MAX];
        if (snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", names.v[i]) >= (int)sizeof sub ||
            snprintf(full, sizeof full, "%s/%.*s", srcdir, (int)(sizeof full - strlen(srcdir) - 2), sub) >=
                (int)sizeof full)
            die("%s: name too long", sub);
        struct stat st;
        if (stat(full, &st))
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (!strcmp(names.v[i], "compile") || !strcmp(names.v[i], "stand"))
                continue;
            if (!strcmp(rel, "arch") && strcmp(names.v[i], machine))
                continue;
            headers(srcdir, sub, out);
        } else {
            size_t n = strlen(names.v[i]);
            if (n > 2 && !strcmp(names.v[i] + n - 2, ".h"))
                ladd(out, sub);
        }
    }
}

static const char *basename_of(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/* the object file of a source file: dir/name.c -> name.o */
static void objname(const char *src, char *out)
{
    snprintf(out, PATH_MAX, "%s", basename_of(src));
    out[strlen(out) - 1] = 'o';
}

/* the template Makefile with the lists filled in */
static void write_makefile(const char *dir, const char *srcdir, const char *S, struct list *active)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/arch/%s/conf/Makefile.%s", srcdir, machine, machine);
    FILE *f = fopen(path, "r");
    if (!f)
        die("%s: %s", path, strerror(errno));
    struct buf b = {0};
    bprintf(&b, "# Makefile: made by config(8) from %s and Makefile.%s; not to be edited\n", confname, machine);
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, f)) != -1) {
        if (n && line[n - 1] == '\n')
            line[--n] = '\0';
        if (!strcmp(line, "%OBJS%")) {
            bputs(&b, "OBJS0 =");
            int col = 7;
            for (int i = 0; i < active->n; i++) {
                char o[PATH_MAX];
                objname(active->v[i], o);
                if (col + (int)strlen(o) + 1 > 70) {
                    bputs(&b, " \\\n\t");
                    col = 8;
                } else {
                    bputs(&b, " ");
                    col++;
                }
                bputs(&b, o);
                col += (int)strlen(o);
            }
            bputs(&b, " ioconf.o\n");
        } else if (!strcmp(line, "%HDRS%")) {
            struct list hs = {0};
            headers(srcdir, "", &hs);
            bputs(&b, "HDRS =");
            for (int i = 0; i < hs.n; i++)
                bprintf(&b, " \\\n\t$(S)/%s", hs.v[i]);
            bputs(&b, "\n");
        } else if (!strcmp(line, "%RULES%")) {
            for (int i = 0; i < active->n; i++) {
                char o[PATH_MAX];
                objname(active->v[i], o);
                int isasm = !strcmp(active->v[i] + strlen(active->v[i]) - 2, ".s");
                bprintf(&b, "%s: $(S)/%s\n\t$(%s) %s-o $@ $(S)/%s\n", o, active->v[i], isasm ? "AS" : "CC",
                        isasm ? "" : "$(CFLAGS) -c ", active->v[i]);
            }
            bputs(&b, "ioconf.o: ioconf.c\n\t$(CC) $(CFLAGS) -c -o $@ ioconf.c\n");
        } else {
            /* %S%, %CONF% and %MACHINE% anywhere in a line */
            const char *p = line;
            while (*p) {
                if (!strncmp(p, "%S%", 3)) {
                    bputs(&b, S);
                    p += 3;
                } else if (!strncmp(p, "%CONF%", 6)) {
                    bputs(&b, confname);
                    p += 6;
                } else if (!strncmp(p, "%MACHINE%", 9)) {
                    bputs(&b, machine);
                    p += 9;
                } else {
                    char one[2] = {*p++, 0};
                    bputs(&b, one);
                }
            }
            bputs(&b, "\n");
        }
    }
    free(line);
    fclose(f);
    put(dir, "Makefile", &b);
    free(b.s);
}

static void link_machine(const char *dir, const char *S)
{
    char link[PATH_MAX], target[PATH_MAX], cur[PATH_MAX];
    snprintf(link, sizeof link, "%s/machine", dir);
    snprintf(target, sizeof target, "%s/arch/%s/include", S, machine);
    ssize_t n = readlink(link, cur, sizeof cur - 1);
    if (n >= 0) {
        cur[n] = '\0';
        if (!strcmp(cur, target))
            return;
        if (unlink(link))
            die("%s: %s", link, strerror(errno));
    } else if (errno != ENOENT) {
        die("%s: not a link to remove or make: %s", link, strerror(errno));
    }
    if (symlink(target, link))
        die("%s: %s", link, strerror(errno));
}

/* ------------------------------------------------------------ main */

static void usage(void)
{
    fprintf(stderr, "usage: config [-s srcdir] [-b builddir] configfile\n");
    exit(2);
}

int main(int argc, char *argv[])
{
    const char *srcdir = "../../..", *builddir = NULL;
    int c;
    while ((c = getopt(argc, argv, "s:b:")) != -1) {
        switch (c) {
        case 's':
            srcdir = optarg;
            break;
        case 'b':
            builddir = optarg;
            break;
        default:
            usage();
        }
    }
    if (optind != argc - 1)
        usage();
    const char *conffile = argv[optind];
    confname = xstrdup(basename_of(conffile));
    if (!isname(confname))
        die("%s: a configuration is named with letters, digits and _", confname);
    char bdir[PATH_MAX];
    if (!builddir) {
        snprintf(bdir, sizeof bdir, "../compile/%s", confname);
        builddir = bdir;
    }
    struct stat st;
    if (stat(srcdir, &st) || !S_ISDIR(st.st_mode))
        die("%s: no source directory (-s)", srcdir);

    /* the machine first, then the files, then the configuration */
    readlines(conffile, find_machine);
    if (!machine)
        die("%s: no `machine' line", conffile);
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/conf/files", srcdir);
    readlines(path, files_line);
    snprintf(path, sizeof path, "%s/arch/%s/conf/files.%s", srcdir, machine, machine);
    readlines(path, files_line);
    if (nerrors)
        return 1;
    readlines(conffile, conf_line);
    if (nerrors)
        return 1;
    if (!rootdev)
        die("%s: no `config bsd' line", conffile);
    check();

    /* which files */
    struct list active = {0};
    for (int i = 0; i < nfentries; i++)
        if (cond_true(&fentries[i])) {
            char o1[PATH_MAX], o2[PATH_MAX];
            objname(fentries[i].path, o1);
            for (int k = 0; k < active.n; k++) {
                objname(active.v[k], o2);
                if (!strcmp(o1, o2)) {
                    cur_file = fentries[i].file;
                    cur_line = fentries[i].line;
                    err("%s and %s would both make %s", active.v[k], fentries[i].path, o1);
                    cur_file = NULL;
                }
            }
            ladd(&active, fentries[i].path);
        }
    if (nerrors)
        return 1;

    mkdirs(builddir);
    char *S = relpath(builddir, srcdir);
    write_options(builddir);
    write_locators(builddir);
    write_ioconf(builddir);
    write_makefile(builddir, srcdir, S, &active);
    link_machine(builddir, S);
    return 0;
}
