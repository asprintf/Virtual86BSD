/*
 * gencat.c - gencat: generate a formatted message catalog
 * (POSIX.1-2024, XCU gencat)
 *
 * Synopsis: gencat catfile msgfile...
 *
 * The catalog is written in the format of the GNU C library, which its
 * catopen() and catgets() read: a header of the magic number 0x960408de,
 * the size and the depth of a hash table (32-bit words in the byte order
 * of this machine), then the table twice, in little-endian and then in
 * big-endian words, each entry (set + 1, message, offset of the text),
 * and the texts, each ended by a NUL byte.  An entry is at depth d of
 * slot ((set + 1) * message) % size; the size is chosen as GNU gencat
 * chooses it (the smallest size times depth, from 1 + messages / 5 on,
 * a later size winning a tie), and the entries go in the order GNU
 * gencat has them (the sets in the order they were first met, the last
 * first, after set NL_SETD; the messages of each in the order of their
 * numbers), so that the same input gives the same file as GNU gencat's.
 * The messages of an existing catalog (in either byte order) are merged
 * after the msgfiles are read, in the order of its table, those that the
 * msgfiles define or delete left out.
 *
 * The message text source: "$set n", "$delset n", "$quote c" (none, or
 * an empty one: no quoting), "$ comment" (and "$" alone), and message
 * lines "m text" (one blank, the rest of the line the text; "m " an
 * empty text; "m" alone deletes message m).  In the text, \n \t \v \b
 * \r \f \\ \ddd (1 to 3 octal digits), and a backslash before another
 * character is left out; a backslash at the end of a line joins the
 * next line.  With a quote character, a text that starts with it ends
 * at the next one that is not escaped (\c for c).
 *
 * Choices where the standard leaves room:
 *  - Without $set, messages are in set NL_SETD (1).
 *  - A message or set number met again (also in a later msgfile) gives
 *    the text that was read last, as messages of a msgfile replace those
 *    of catfile.  Numbers out of order are taken.
 *  - $delset deletes the set from catfile and from the msgfiles read so
 *    far; a message line without its blank deletes the message likewise.
 *  - Other lines starting with '$' are reported (a warning) and left out.
 *    Lines starting with something else than '$', a digit or a blank
 *    character, an invalid number, a quoted text without its closing
 *    quote, and a catfile that is not a catalog are errors: the status is
 *    1 and the catalog is not written.
 *  - "-" as catfile writes to the standard output (no catalog is read).
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC 0x960408deu

static int errors;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("gencat: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fputs("gencat: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

/* ---- the catalog ---- */

/* The sets are kept in the order GNU gencat keeps them: a new set goes in
   front, and set NL_SETD is there from the start; the messages of a set
   are kept in the order of their numbers.  What the msgfiles delete is
   remembered, so that the messages of the existing catalog, merged
   after the msgfiles have been read (as GNU gencat does), stay out. */

struct msg {
    unsigned long num;
    char *text;
    size_t len;
};
struct set {
    unsigned long num;
    struct msg *m;
    size_t n, cap;
    unsigned long *gone;        /* messages deleted */
    size_t ngone;
    bool deleted;               /* by $delset */
};
static struct set *sets;
static size_t nsets, capsets;
static size_t counted;          /* messages as GNU gencat counts them */

static struct set *find_set(unsigned long num, bool make)
{
    for (size_t i = 0; i < nsets; i++)
        if (sets[i].num == num)
            return &sets[i];
    if (!make)
        return NULL;
    if (nsets == capsets) {
        capsets = capsets ? capsets * 2 : 16;
        sets = xrealloc(sets, capsets * sizeof *sets);
    }
    memmove(&sets[1], &sets[0], nsets * sizeof *sets);
    nsets++;
    sets[0] = (struct set){ num, NULL, 0, 0, NULL, 0, false };
    return &sets[0];
}

static void del_set(unsigned long num)
{
    struct set *s = find_set(num, true);
    for (size_t i = 0; i < s->n; i++)
        free(s->m[i].text);
    s->n = 0;
    s->deleted = true;
}

static struct msg *find_msg(struct set *s, unsigned long num, size_t *at)
{
    size_t i = 0;
    while (i < s->n && s->m[i].num < num)
        i++;
    *at = i;
    return i < s->n && s->m[i].num == num ? &s->m[i] : NULL;
}

static void put_msg(unsigned long set, unsigned long num, char *text, size_t len)
{
    struct set *s = find_set(set, true);
    size_t i;
    struct msg *m = find_msg(s, num, &i);
    if (m) {
        free(m->text);
        m->text = text;
        m->len = len;
        return;
    }
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->m = xrealloc(s->m, s->cap * sizeof *s->m);
    }
    memmove(&s->m[i + 1], &s->m[i], (s->n - i) * sizeof *s->m);
    s->n++;
    s->m[i] = (struct msg){ num, text, len };
    counted++;
}

static void del_msg(unsigned long set, unsigned long num)
{
    struct set *s = find_set(set, true);
    size_t i;
    if (find_msg(s, num, &i)) {
        free(s->m[i].text);
        memmove(&s->m[i], &s->m[i + 1], (s->n - i - 1) * sizeof *s->m);
        s->n--;
    }
    s->gone = xrealloc(s->gone, (s->ngone + 1) * sizeof *s->gone);
    s->gone[s->ngone++] = num;
    counted++;                  /* GNU gencat keeps an empty message for it */
}

/* a message of the existing catalog: kept unless the msgfiles have it,
   or deleted it or its set */
static void old_msg(unsigned long set, unsigned long num, const char *text, size_t len)
{
    struct set *s = find_set(set, true);
    size_t i;
    if (s->deleted || find_msg(s, num, &i))
        return;
    for (size_t k = 0; k < s->ngone; k++)
        if (s->gone[k] == num)
            return;
    char *t = xmalloc(len + 1);
    memcpy(t, text, len + 1);
    put_msg(set, num, t, len);
}

/* ---- reading an existing catalog ---- */

static uint32_t get32(const unsigned char *p, bool little)
{
    return little ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24
                  : (uint32_t)p[3] | (uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24;
}

static bool read_catalog(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT)
            return true;
        fprintf(stderr, "gencat: %s: %s\n", path, strerror(errno));
        return false;
    }
    unsigned char *d = NULL;
    size_t n = 0, cap = 0, k;
    unsigned char buf[65536];
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) {
        if (n + k > cap) {
            cap = (n + k) * 2;
            d = xrealloc(d, cap);
        }
        memcpy(d + n, buf, k);
        n += k;
    }
    bool rerr = ferror(f);
    fclose(f);
    if (rerr) {
        fprintf(stderr, "gencat: %s: %s\n", path, strerror(errno));
        free(d);
        return false;
    }
    if (n == 0) {               /* an empty file: a new catalog */
        free(d);
        return true;
    }
    bool little;
    if (n >= 12 && get32(d, true) == MAGIC)
        little = true;
    else if (n >= 12 && get32(d, false) == MAGIC)
        little = false;
    else
        goto bad;
    uint64_t size = get32(d + 4, little), depth = get32(d + 8, little);
    uint64_t entries = size * depth * 3;
    if (size == 0 || entries > (n - 12) / 8)
        goto bad;
    /* the little-endian table comes first */
    const unsigned char *tab = d + 12;
    const unsigned char *strs = d + 12 + 8 * entries;
    size_t strsize = n - 12 - 8 * entries;
    for (uint64_t i = 0; i < size * depth; i++) {
        uint32_t s = get32(tab + 12 * i, true), m = get32(tab + 12 * i + 4, true), o = get32(tab + 12 * i + 8, true);
        if (s == 0)
            continue;
        if (o >= strsize || !memchr(strs + o, '\0', strsize - o))
            goto bad;
        old_msg(s - 1, m, (const char *)strs + o, strlen((const char *)strs + o));
    }
    free(d);
    return true;
bad:
    fprintf(stderr, "gencat: %s: not a message catalog\n", path);
    free(d);
    return false;
}

/* ---- the message text source ---- */

struct source {
    FILE *f;
    const char *name;
    long line;
    char *buf;
    size_t cap;
};

static void error(struct source *s, const char *what)
{
    fprintf(stderr, "gencat: %s:%ld: %s\n", s->name, s->line, what);
    errors++;
}

static char *get_line(struct source *s, size_t *len)
{
    ssize_t n = getline(&s->buf, &s->cap, s->f);
    if (n < 0)
        return NULL;
    s->line++;
    if (n > 0 && s->buf[n - 1] == '\n')
        s->buf[--n] = '\0';
    *len = (size_t)n;
    return s->buf;
}

/* a number at *p (digits); false if there are none or it is out of range */
static bool number(char **p, unsigned long max, unsigned long *v)
{
    char *s = *p;
    if (*s < '0' || *s > '9')
        return false;
    unsigned long r = 0;
    while (*s >= '0' && *s <= '9') {
        if (r > (max - (unsigned long)(*s - '0')) / 10)
            return false;
        r = r * 10 + (unsigned long)(*s - '0');
        s++;
    }
    *p = s;
    *v = r;
    return r >= 1;
}

static bool blank(int c)
{
    return c == ' ' || c == '\t';
}

/* the text from p (and the lines it continues to), escapes processed */
static char *text(struct source *s, char *p, int quote, size_t *outlen)
{
    size_t cap = strlen(p) + 64, n = 0;
    char *out = xmalloc(cap);
    bool quoted = quote && *p == quote;
    if (quoted)
        p++;
    for (;;) {
        if (*p == '\0') {
            if (quoted)
                error(s, "a quoted text without its closing quote");
            break;
        }
        if (quoted && *p == quote) {
            p++;
            break;
        }
        if (n + 4 >= cap) {
            cap *= 2;
            out = xrealloc(out, cap);
        }
        if (*p != '\\') {
            out[n++] = *p++;
            continue;
        }
        p++;
        switch (*p) {
        case '\0': {
            /* a continuation line */
            size_t l;
            char *next = get_line(s, &l);
            if (!next) {
                out[n] = '\0';
                *outlen = n;
                return out;
            }
            p = next;
            if (strlen(p) + n + 64 > cap) {
                cap = strlen(p) + n + 64;
                out = xrealloc(out, cap);
            }
            continue;
        }
        case 'n': out[n++] = '\n'; p++; break;
        case 't': out[n++] = '\t'; p++; break;
        case 'v': out[n++] = '\v'; p++; break;
        case 'b': out[n++] = '\b'; p++; break;
        case 'r': out[n++] = '\r'; p++; break;
        case 'f': out[n++] = '\f'; p++; break;
        default:
            if (*p >= '0' && *p <= '7') {
                unsigned v = 0;
                for (int k = 0; k < 3 && *p >= '0' && *p <= '7'; k++)
                    v = v * 8 + (unsigned)(*p++ - '0');
                out[n++] = (char)v;
            } else {
                out[n++] = *p++;        /* \\, \c: c */
            }
            break;
        }
    }
    out[n] = '\0';
    *outlen = n;
    return out;
}

static void read_source(struct source *s)
{
    unsigned long set = NL_SETD;
    int quote = 0;
    size_t len;
    char *l;
    while ((l = get_line(s, &len))) {
        if (len == 0)
            continue;
        if (l[0] == '$') {
            char *p = l + 1;
            if (*p == '\0' || blank(*p))
                continue;                       /* a comment */
            char *kw = p;
            while (*p && !blank(*p))
                p++;
            size_t kl = (size_t)(p - kw);
            if (kl == 3 && strncmp(kw, "set", 3) == 0 && blank(*p)) {
                p++;
                while (blank(*p))
                    p++;
                unsigned long v;
                if (!number(&p, NL_SETMAX, &v) || (*p && !blank(*p))) {
                    error(s, "an invalid set number");
                } else {
                    set = v;
                    find_set(v, true);  /* now, for the order of the sets */
                }
            } else if (kl == 6 && strncmp(kw, "delset", 6) == 0 && blank(*p)) {
                p++;
                while (blank(*p))
                    p++;
                unsigned long v;
                if (!number(&p, NL_SETMAX, &v) || (*p && !blank(*p)))
                    error(s, "an invalid set number");
                else
                    del_set(v);
            } else if (kl == 5 && strncmp(kw, "quote", 5) == 0) {
                if (*p && blank(*p) && p[1])
                    quote = (unsigned char)p[1];
                else
                    quote = 0;
            } else {
                fprintf(stderr, "gencat: %s:%ld: warning: an unknown directive, left out\n", s->name, s->line);
            }
            continue;
        }
        if (l[0] >= '0' && l[0] <= '9') {
            char *p = l;
            unsigned long m;
            if (!number(&p, NL_MSGMAX, &m) || (*p && !blank(*p))) {
                error(s, "an invalid message number");
                continue;
            }
            if (*p == '\0') {
                del_msg(set, m);
                continue;
            }
            size_t tl;
            char *t = text(s, p + 1, quote, &tl);
            if (tl > NL_TEXTMAX)
                error(s, "a message longer than NL_TEXTMAX");
            put_msg(set, m, t, tl);
            continue;
        }
        if (blank(l[0])) {
            char *p = l;
            while (blank(*p))
                p++;
            if (*p == '\0')
                continue;                       /* blanks only: an empty line */
        }
        error(s, "a line that is neither a directive nor a message");
    }
    if (ferror(s->f)) {
        fprintf(stderr, "gencat: %s: %s\n", s->name, strerror(errno));
        errors++;
    }
    free(s->buf);
}

/* ---- writing ---- */

static void put32(unsigned char *p, uint32_t v, bool little)
{
    for (int i = 0; i < 4; i++)
        p[little ? i : 3 - i] = (unsigned char)(v >> (8 * i));
}

static bool write_catalog(const char *path)
{
    /* the size of the hash table, as GNU gencat chooses it: from 1 + the
       messages / 5, counting each deletion as a message, as it does */
    uint64_t best_total = UINT64_MAX, best_size = 1, best_depth = 1;
    size_t *deep = NULL;
    for (uint64_t size = 1 + counted / 5; size <= best_total; size++) {
        deep = xrealloc(deep, (size_t)size * sizeof *deep);
        memset(deep, 0, (size_t)size * sizeof *deep);
        uint64_t depth = 1;
        for (size_t i = 0; i < nsets; i++)
            for (size_t k = 0; k < sets[i].n; k++) {
                uint64_t idx = ((uint64_t)(sets[i].num + 1) * sets[i].m[k].num) % size;
                if (++deep[idx] > depth)
                    depth = deep[idx];
            }
        if (depth * size <= best_total) {
            best_total = depth * size;
            best_size = size;
            best_depth = depth;
        }
    }
    free(deep);
    size_t ent = (size_t)(best_size * best_depth);
    if (ent > SIZE_MAX / 24) {
        fputs("gencat: the catalog is too large\n", stderr);
        return false;
    }
    uint32_t *tab = calloc(ent * 3, sizeof *tab);
    if (!tab) {
        fputs("gencat: out of memory\n", stderr);
        exit(1);
    }
    size_t strsize = 0;
    for (size_t i = 0; i < nsets; i++)
        for (size_t k = 0; k < sets[i].n; k++)
            strsize += sets[i].m[k].len + 1;
    char *strs = xmalloc(strsize);
    size_t off = 0;
    for (size_t i = 0; i < nsets; i++)
        for (size_t k = 0; k < sets[i].n; k++) {
            uint64_t idx = ((uint64_t)(sets[i].num + 1) * sets[i].m[k].num) % best_size;
            size_t d = 0;
            while (tab[(d * best_size + idx) * 3] != 0)
                d++;
            uint32_t *e = &tab[(d * best_size + idx) * 3];
            e[0] = (uint32_t)(sets[i].num + 1);
            e[1] = (uint32_t)sets[i].m[k].num;
            e[2] = (uint32_t)off;
            memcpy(strs + off, sets[i].m[k].text, sets[i].m[k].len + 1);
            off += sets[i].m[k].len + 1;
        }
    if (strsize > UINT32_MAX) {
        fputs("gencat: the catalog is too large\n", stderr);
        return false;
    }
    size_t fsize = 12 + 24 * ent + strsize;
    unsigned char *out = xmalloc(fsize);
    uint32_t h[3] = { MAGIC, (uint32_t)best_size, (uint32_t)best_depth };
    memcpy(out, h, 12);                         /* the byte order of this machine */
    for (size_t i = 0; i < ent * 3; i++) {
        put32(out + 12 + 4 * i, tab[i], true);
        put32(out + 12 + 12 * ent + 4 * i, tab[i], false);
    }
    memcpy(out + 12 + 24 * ent, strs, strsize);
    free(tab);
    free(strs);

    bool ok;
    if (strcmp(path, "-") == 0) {
        ok = fwrite(out, 1, fsize, stdout) == fsize && fflush(stdout) == 0;
        if (!ok)
            fprintf(stderr, "gencat: standard output: %s\n", strerror(errno));
    } else {
        FILE *f = fopen(path, "wb");
        ok = f && fwrite(out, 1, fsize, f) == fsize;
        if (f && fclose(f) != 0)
            ok = false;
        if (!ok)
            fprintf(stderr, "gencat: %s: %s\n", path, strerror(errno));
    }
    free(out);
    return ok;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int a = 1;
    if (a < argc && strcmp(argv[a], "--") == 0)
        a++;
    else if (a < argc && argv[a][0] == '-' && argv[a][1] != '\0') {
        fprintf(stderr, "gencat: unknown option: %s\n", argv[a]);
        fputs("usage: gencat catfile msgfile...\n", stderr);
        return 2;
    }
    if (argc - a < 2) {
        fputs("usage: gencat catfile msgfile...\n", stderr);
        return 2;
    }
    const char *cat = argv[a];
    find_set(NL_SETD, true);
    for (int i = a + 1; i < argc; i++) {
        struct source s = { NULL, argv[i], 0, NULL, 0 };
        if (strcmp(argv[i], "-") == 0) {
            s.f = stdin;
            s.name = "standard input";
        } else if (!(s.f = fopen(argv[i], "r"))) {
            fprintf(stderr, "gencat: %s: %s\n", argv[i], strerror(errno));
            errors++;
            continue;
        }
        read_source(&s);
        if (s.f != stdin)
            fclose(s.f);
    }
    if (errors)
        return 1;
    if (strcmp(cat, "-") != 0 && !read_catalog(cat))
        return 1;
    return write_catalog(cat) ? 0 : 1;
}
