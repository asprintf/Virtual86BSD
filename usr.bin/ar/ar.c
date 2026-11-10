/*
 * ar.c - ar: create and maintain library archives (POSIX.1-2024, XCU ar)
 *
 * Synopsis: ar -d [-v] archive file...
 *           ar -m [-v] [-a|-b|-i posname] archive file...
 *           ar -p [-v] [-s] archive [file...]
 *           ar -q [-cv] archive file...
 *           ar -r [-cuv] [-a|-b|-i posname] archive file...
 *           ar -t [-v] [-s] archive [file...]
 *           ar -x [-v] [-sCT] archive [file...]
 *
 * The archives are those the link editor reads (arch.c): the format of
 * GNU ar and System V, with a symbol table of the object members' global
 * symbols whenever one of the members is an ELF object file (elf.c), the
 * same bytes as GNU ar writes with its U modifier (real dates, owners and
 * modes) but for the date of the symbol table.  An archive that is
 * changed is written to a new file in its directory, which then replaces
 * it (keeping its permissions).
 *
 * Choices where the standard leaves room:
 *  - A first argument without "-" made of the letters of the options is
 *    taken as those options (ar rv lib.a x.o), as historical ar does.
 *  - -s alone regenerates the symbol table (as ranlib); with -p, -t and
 *    -x it does so too, after their work.  Every change (-d, -m, -q, -r)
 *    regenerates it.
 *  - The date of the symbol table is the time of the change.
 *  - Files in the archive with the same name: an operand matches the
 *    first; -q adds names that are there already.
 *  - -r without file operands: the archive is made if it is not there,
 *    else only its symbol table regenerated.
 *  - With operands, -p, -t and -x take them in their order, and -m moves
 *    them in their order (as GNU ar); without, the order of the archive.
 *  - -x: the files get the mode of their member (less the file mode
 *    creation mask), and the time of the extraction; -C leaves a file
 *    that exists as it is (with a message, status 1); a name longer than
 *    {NAME_MAX} is an error without -T, cut to {NAME_MAX} with it.
 *  - -tv: "%s %u/%u %u %s %s\n" with the mode string, owner, group,
 *    size, the date as date +"%b %e %H:%M %Y", and the name.
 *  - An operand not in the archive (-d, -m, -p, -t, -x) is reported and
 *    the status is 1; the others are still done.  A posname not in the
 *    archive (or moved itself) is as if not given: the end (as GNU ar).
 *  - A file operand of -r or -q that cannot be read: the archive is not
 *    changed (status 1).
 *  - Values too large for their header field (an owner above 999999) are
 *    written as 0.  Names with a newline are refused.
 *  - Usage errors exit with status 2, other errors with status 1.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "arch.h"
#include "elf.h"

#include <errno.h>
#include <fcntl.h>
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
static bool o_c, o_u, o_v, o_s, o_C, o_T;
static char op;                 /* d m p q r t x, or s alone */
static char pos;                /* a b (i is b), or 0 */
static const char *posname;

static void usage(void)
{
    fputs("usage: ar -d [-v] archive file...\n"
          "       ar -m [-v] [-a|-b|-i posname] archive file...\n"
          "       ar -p [-v] [-s] archive [file...]\n"
          "       ar -q [-cv] archive file...\n"
          "       ar -r [-cuv] [-a|-b|-i posname] archive file...\n"
          "       ar -t [-v] [-s] archive [file...]\n"
          "       ar -x [-v] [-sCT] archive [file...]\n", stderr);
    exit(2);
}

static const char *base(const char *p)
{
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/')
        n--;
    const char *s = p + n;
    while (s > p && s[-1] != '/')
        s--;
    return s;
}

/* the last component of an operand (static buffer) */
static const char *last(const char *p)
{
    static char buf[PATH_MAX];
    snprintf(buf, sizeof buf, "%s", base(p));
    size_t n = strlen(buf);
    while (n > 1 && buf[n - 1] == '/')
        buf[--n] = '\0';
    return buf;
}

static long find(const struct archive *a, const char *name)
{
    const char *b = last(name);
    for (size_t i = 0; i < a->n; i++)
        if (strcmp(a->m[i].name, b) == 0)
            return (long)i;
    return -1;
}

/* ---- the archive file ---- */

static const char *arpath;
static bool exists;
static struct stat arst;

static bool load(struct archive *a)
{
    memset(a, 0, sizeof *a);
    if (stat(arpath, &arst) != 0) {
        if (errno != ENOENT) {
            fprintf(stderr, "ar: %s: %s\n", arpath, strerror(errno));
            return false;
        }
        exists = false;
        return true;
    }
    exists = true;
    size_t n;
    unsigned char *d = ar_slurp(arpath, &n);
    if (!d) {
        fprintf(stderr, "ar: %s: %s\n", arpath, strerror(errno));
        return false;
    }
    const char *why;
    bool ok = ar_parse(a, d, n, &why);
    free(d);
    if (!ok)
        fprintf(stderr, "ar: %s: %s\n", arpath, why);
    return ok;
}

static bool save(const struct archive *a)
{
    size_t n;
    unsigned char *d = ar_build(a, &n, (long long)time(NULL));
    if (!d) {
        fputs("ar: out of memory\n", stderr);
        return false;
    }
    char tmp[PATH_MAX];
    const char *b = base(arpath);
    int dl = (int)(b - arpath);
    if (snprintf(tmp, sizeof tmp, "%.*sar.XXXXXX", dl, arpath) >= (int)sizeof tmp) {
        fprintf(stderr, "ar: %s: name too long\n", arpath);
        free(d);
        return false;
    }
    int fd = mkstemp(tmp);
    if (fd < 0) {
        fprintf(stderr, "ar: %s: %s\n", tmp, strerror(errno));
        free(d);
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
    free(d);
    if (ok) {
        mode_t m;
        if (exists) {
            m = arst.st_mode & 07777;
        } else {
            mode_t mask = umask(0);
            umask(mask);
            m = 0666 & ~mask;
        }
        fchmod(fd, m);
    }
    if (close(fd) != 0)
        ok = false;
    if (!ok || rename(tmp, arpath) != 0) {
        fprintf(stderr, "ar: %s: %s\n", arpath, strerror(errno));
        unlink(tmp);
        return false;
    }
    return true;
}

/* a member from the file at path */
static bool member_from(const char *path, struct ar_member *m)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "ar: %s: %s\n", path, strerror(errno));
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "ar: %s: not a regular file\n", path);
        return false;
    }
    const char *b = last(path);
    if (strchr(b, '\n')) {
        fprintf(stderr, "ar: %s: a name with a newline\n", path);
        return false;
    }
    size_t n;
    unsigned char *d = ar_slurp(path, &n);
    if (!d) {
        fprintf(stderr, "ar: %s: %s\n", path, strerror(errno));
        return false;
    }
    m->name = strdup(b);
    if (!m->name) {
        free(d);
        fputs("ar: out of memory\n", stderr);
        return false;
    }
    m->mtime = (long long)st.st_mtime;
    m->uid = (long)st.st_uid;
    m->gid = (long)st.st_gid;
    m->mode = (unsigned long)st.st_mode;
    m->data = d;
    m->size = n;
    return true;
}

static void insert_at(struct archive *a, size_t at, const struct ar_member *m)
{
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        a->m = realloc(a->m, a->cap * sizeof *a->m);
        if (!a->m) {
            fputs("ar: out of memory\n", stderr);
            exit(1);
        }
    }
    memmove(&a->m[at + 1], &a->m[at], (a->n - at) * sizeof *a->m);
    a->m[at] = *m;
    a->n++;
}

static struct ar_member take(struct archive *a, size_t i)
{
    struct ar_member m = a->m[i];
    memmove(&a->m[i], &a->m[i + 1], (a->n - i - 1) * sizeof *a->m);
    a->n--;
    return m;
}

/* the place for new files: -a after posname, -b/-i before it, else the end */
static bool where(const struct archive *a, size_t *at)
{
    if (!pos) {
        *at = a->n;
        return true;
    }
    long p = find(a, posname);
    *at = p < 0 ? a->n : (size_t)p + (pos == 'a');     /* not there: the end */
    return true;
}

/* ---- the operations ---- */

static bool *named;             /* which operands matched */

static void unmatched(char **ops, int nops)
{
    for (int k = 0; k < nops; k++)
        if (!named[k]) {
            fprintf(stderr, "ar: %s: not in the archive\n", ops[k]);
            status = 1;
        }
}

static void mode_string(unsigned long mode, char *s)
{
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++)
        s[i] = mode & (0400u >> i) ? rwx[i] : '-';
    if (mode & 04000)
        s[2] = mode & 0100 ? 's' : 'S';
    if (mode & 02000)
        s[5] = mode & 010 ? 's' : 'S';
    if (mode & 01000)
        s[8] = mode & 01 ? 't' : 'T';
    s[9] = '\0';
}

/* the members to work on: those of the operands in their order, or all;
   false at the end */
static bool next_member(const struct archive *a, char **ops, int nops, size_t *j, size_t *i, const char **as)
{
    if (nops == 0) {
        if (*j >= a->n)
            return false;
        *i = (*j)++;
        *as = a->m[*i].name;
        return true;
    }
    while (*j < (size_t)nops) {
        long f = find(a, ops[*j]);
        size_t k = (*j)++;
        if (f >= 0) {
            named[k] = true;
            *i = (size_t)f;
            *as = ops[k];
            return true;
        }
    }
    return false;
}

static void list(const struct archive *a, char **ops, int nops)
{
    size_t j = 0, i;
    const char *as;
    while (next_member(a, ops, nops, &j, &i, &as)) {
        if (!o_v) {
            printf("%s\n", as);
            continue;
        }
        const struct ar_member *m = &a->m[i];
        char ms[10], date[64] = "";
        mode_string(m->mode, ms);
        time_t t = (time_t)m->mtime;
        struct tm *tm = localtime(&t);
        if (tm)
            strftime(date, sizeof date, "%b %e %H:%M %Y", tm);
        printf("%s %lu/%lu %zu %s %s\n", ms, (unsigned long)m->uid, (unsigned long)m->gid, m->size, date, as);
    }
    unmatched(ops, nops);
}

static void print(const struct archive *a, char **ops, int nops)
{
    size_t j = 0, i;
    const char *as;
    while (next_member(a, ops, nops, &j, &i, &as)) {
        if (o_v)
            printf("\n<%s>\n\n", as);
        fflush(stdout);
        fwrite(a->m[i].data, 1, a->m[i].size, stdout);
    }
    unmatched(ops, nops);
}

static void extract(const struct archive *a, char **ops, int nops)
{
    mode_t mask = umask(0);
    umask(mask);
    size_t j = 0, i;
    const char *as;
    while (next_member(a, ops, nops, &j, &i, &as)) {
        const struct ar_member *m = &a->m[i];
        char name[PATH_MAX];
        snprintf(name, sizeof name, "%s", m->name);
        if (!name[0] || strchr(name, '/') || strcmp(name, ".") == 0 || strcmp(name, "..") == 0
            || strchr(name, '\n')) {
            fprintf(stderr, "ar: %s: a name that cannot be extracted\n", m->name);
            status = 1;
            continue;
        }
        long max = pathconf(".", _PC_NAME_MAX);
        if (max > 0 && (long)strlen(name) > max) {
            if (!o_T) {
                fprintf(stderr, "ar: %s: name too long (-T would cut it)\n", m->name);
                status = 1;
                continue;
            }
            name[max] = '\0';
        }
        int flags = O_WRONLY | O_CREAT | O_TRUNC;
        if (o_C)
            flags |= O_EXCL;
        mode_t fm = (mode_t)(m->mode & 07777 & ~mask);
        int fd = open(name, flags, fm ? fm : 0600);
        if (fd < 0) {
            fprintf(stderr, "ar: %s: %s\n", name, errno == EEXIST ? "exists, not replaced (-C)" : strerror(errno));
            status = 1;
            continue;
        }
        size_t off = 0;
        bool ok = true;
        while (off < m->size) {
            ssize_t k = write(fd, m->data + off, m->size - off);
            if (k < 0) {
                if (errno == EINTR)
                    continue;
                ok = false;
                break;
            }
            off += (size_t)k;
        }
        if (ok)
            fchmod(fd, fm);
        if (close(fd) != 0)
            ok = false;
        if (!ok) {
            fprintf(stderr, "ar: %s: %s\n", name, strerror(errno));
            status = 1;
            continue;
        }
        if (o_v)
            printf("x - %s\n", as);
    }
    unmatched(ops, nops);
}

static bool delete(struct archive *a, char **ops, int nops)
{
    for (int k = 0; k < nops; k++) {
        long i = find(a, ops[k]);
        if (i < 0) {
            fprintf(stderr, "ar: %s: not in the archive\n", ops[k]);
            status = 1;
            continue;
        }
        struct ar_member m = take(a, (size_t)i);
        free(m.name);
        free(m.data);
        if (o_v)
            printf("d - %s\n", ops[k]);
    }
    return true;
}

static bool move(struct archive *a, char **ops, int nops)
{
    /* the members named, in the order of the archive, taken out */
    struct ar_member *moved = malloc(sizeof *moved * (size_t)(nops ? nops : 1));
    bool *in = calloc(a->n ? a->n : 1, sizeof *in);
    if (!moved || !in) {
        fputs("ar: out of memory\n", stderr);
        exit(1);
    }
    for (int k = 0; k < nops; k++) {
        long i = find(a, ops[k]);
        if (i < 0) {
            fprintf(stderr, "ar: %s: not in the archive\n", ops[k]);
            status = 1;
        } else {
            in[i] = true;
        }
    }
    /* the position: relative to posname among the members that stay */
    int nm = 0;
    struct archive rest = { NULL, 0, 0, false };
    rest.m = malloc(sizeof *rest.m * (a->n ? a->n : 1));
    if (!rest.m) {
        fputs("ar: out of memory\n", stderr);
        exit(1);
    }
    /* those moved in the order of the operands (as GNU ar) */
    {
        bool *done = calloc(a->n ? a->n : 1, sizeof *done);
        if (!done) {
            fputs("ar: out of memory\n", stderr);
            exit(1);
        }
        nm = 0;
        for (int k = 0; k < nops; k++) {
            long i = find(a, ops[k]);
            if (i >= 0 && !done[i]) {
                done[i] = true;
                moved[nm++] = a->m[i];
            }
        }
        for (size_t i = 0; i < a->n; i++)
            if (!in[i])
                rest.m[rest.n++] = a->m[i];
        free(done);
    }
    rest.cap = a->n ? a->n : 1;
    size_t at = rest.n;
    if (pos) {
        long p = find(&rest, posname);
        if (p >= 0)             /* not there, or moved itself: the end */
            at = (size_t)p + (pos == 'a');
    }
    for (int k = 0; k < nm; k++)
        insert_at(&rest, at + (size_t)k, &moved[k]);
    free(a->m);
    a->m = rest.m;
    a->n = rest.n;
    a->cap = rest.cap;
    if (o_v)
        for (int k = 0; k < nops; k++)
            if (find(a, ops[k]) >= 0)
                printf("m - %s\n", ops[k]);
    free(moved);
    free(in);
    return true;
}

static bool replace(struct archive *a, char **ops, int nops, bool quick)
{
    size_t at;
    if (!quick && !where(a, &at))
        return false;
    if (quick)
        at = a->n;
    /* the files first: one that cannot be read changes nothing */
    struct ar_member *new = calloc((size_t)(nops ? nops : 1), sizeof *new);
    bool *skip = calloc((size_t)(nops ? nops : 1), sizeof *skip);
    if (!new || !skip) {
        fputs("ar: out of memory\n", stderr);
        exit(1);
    }
    for (int k = 0; k < nops; k++) {
        long i = quick ? -1 : find(a, ops[k]);
        if (i >= 0 && o_u) {
            struct stat st;
            if (stat(ops[k], &st) == 0 && (long long)st.st_mtime < a->m[i].mtime) {
                skip[k] = true;
                continue;
            }
        }
        if (!member_from(ops[k], &new[k])) {
            for (int j = 0; j < k; j++) {
                free(new[j].name);
                free(new[j].data);
            }
            free(new);
            free(skip);
            return false;
        }
    }
    for (int k = 0; k < nops; k++) {
        if (skip[k])
            continue;
        struct ar_member m = new[k];
        long i = quick ? -1 : find(a, ops[k]);
        if (i >= 0) {
            free(a->m[i].name);
            free(a->m[i].data);
            a->m[i] = m;
            if (o_v)
                printf("r - %s\n", ops[k]);
        } else {
            insert_at(a, at++, &m);
            if (o_v)
                printf("a - %s\n", ops[k]);
        }
    }
    free(new);
    free(skip);
    return true;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    /* the historical key letters: "ar rv lib.a x.o" */
    char *keys = NULL;
    if (argc > 1 && argv[1][0] != '-' && argv[1][0] && strspn(argv[1], "dmpqrtxabicCsTuv") == strlen(argv[1])) {
        size_t l = strlen(argv[1]);
        keys = malloc(l + 2);
        if (!keys)
            return 1;
        keys[0] = '-';
        memcpy(keys + 1, argv[1], l + 1);
        argv[1] = keys;
    }
    int c;
    while ((c = getopt(argc, argv, "dmpqrtxabicCsTuv")) != -1) {
        switch (c) {
        case 'd': case 'm': case 'p': case 'q': case 'r': case 't': case 'x':
            if (op && op != c)
                usage();
            op = (char)c;
            break;
        case 'a': case 'b': case 'i':
            if (pos && pos != (c == 'i' ? 'b' : c))
                usage();
            pos = c == 'i' ? 'b' : (char)c;
            break;
        case 'c': o_c = true; break;
        case 'C': o_C = true; break;
        case 's': o_s = true; break;
        case 'T': o_T = true; break;
        case 'u': o_u = true; break;
        case 'v': o_v = true; break;
        default: usage();
        }
    }
    if (!op && o_s)
        op = 's';
    if (!op)
        usage();
    if (pos && op != 'm' && op != 'r')
        usage();
    if (pos) {
        if (optind >= argc)
            usage();
        posname = argv[optind++];
    }
    if (optind >= argc)
        usage();
    arpath = argv[optind++];
    char **ops = argv + optind;
    int nops = argc - optind;
    if ((op == 'd' || op == 'm' || op == 'q') && nops == 0)
        usage();
    if (op == 's' && nops > 0)
        usage();
    named = calloc((size_t)(nops ? nops : 1), sizeof *named);
    if (!named)
        return 1;

    struct archive a;
    if (!load(&a))
        return 1;
    if (!exists && op != 'r' && op != 'q') {
        fprintf(stderr, "ar: %s: %s\n", arpath, strerror(ENOENT));
        return 1;
    }
    if (!exists && !o_c)
        fprintf(stderr, "ar: creating %s\n", arpath);
    bool change = true, ok = true;
    switch (op) {
    case 't': list(&a, ops, nops); change = o_s; break;
    case 'p': print(&a, ops, nops); change = o_s; break;
    case 'x': extract(&a, ops, nops); change = o_s; break;
    case 'd': ok = delete(&a, ops, nops); break;
    case 'm': ok = move(&a, ops, nops); break;
    case 'q': ok = replace(&a, ops, nops, true); break;
    case 'r': ok = replace(&a, ops, nops, false); break;
    case 's': break;
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "ar: standard output: %s\n", strerror(errno));
        status = 1;
    }
    if (!ok)
        status = 1;
    else if (change && !save(&a))
        status = 1;
    ar_free(&a);
    free(named);
    free(keys);
    return status;
}
