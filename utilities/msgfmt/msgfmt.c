/*
 * msgfmt.c - msgfmt: create messages objects from portable messages
 * object source files (POSIX.1-2024, XCU msgfmt)
 *
 * Synopsis: msgfmt [-cfSv] [-D dir] [-o outputfile] pathname...
 *
 * The dot-po files are read by po.c.  The messages objects written are
 * those of GNU gettext (the format the gettext functions of the C
 * libraries read, and gettext and ngettext here): the 32-bit words in
 * the byte order of this machine, the strings sorted by their bytes, and
 * a hash table, all as GNU msgfmt writes them, so the same input gives
 * the same file.
 *
 * Choices where the standard leaves room:
 *  - Without -o, the messages of domain D go to D.mo in the working
 *    directory, with or without -S; -S adds .mo to the name of -o when
 *    it does not end so.
 *  - A pathname that cannot be opened, if it is relative, is looked for
 *    in the -D directories, in their order; "-" is the standard input.
 *  - A domain with nothing to write (no messages, or all of them left
 *    out) gets no file, as with GNU msgfmt.
 *  - Messages with an empty translation (the first form empty) are left
 *    out; so are fuzzy ones without -f, but the header (msgid "") is kept
 *    even when fuzzy.
 *  - Errors: a message defined twice in a domain; plural forms without
 *    "nplurals=...; plural=..." in the domain's header, or not as many
 *    as nplurals; a header whose plural expression is not valid.  Then
 *    nothing is written and the status is 1.
 *  - The checks of -c (with -v or without) are those of the standard:
 *    a newline at the start or end of one string and not the other, and
 *    for c-format messages (#, c-format; or no flag and a % in the
 *    msgid) a different number of conversions, or of argument types
 *    (positions with %n$ are taken); msgstr[0] is compared with msgid,
 *    the other forms with msgid_plural.  -v writes the counts of
 *    translated, fuzzy and untranslated messages to the standard error.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "plural.h"
#include "po.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool o_c, o_f, o_S, o_v;
static const char *o_o;
static char **dirs;
static int ndirs;
static int errors;
static long n_translated, n_fuzzy, n_untranslated;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("msgfmt: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fputs("msgfmt: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static char *xmemdup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* ---- the messages of each domain ---- */

struct entry {
    char *key;                  /* [ctxt \004] msgid [\0 msgid_plural] */
    size_t keylen;              /* to the end of msgid (the sort key) */
    size_t origlen;             /* the whole original string */
    char *trans;                /* the forms, each ended by \0 but the last */
    size_t translen;
    const char *file;
    long line;
};

struct domain {
    char *name;
    struct entry *e;
    size_t n, cap;
    struct entry *seen;         /* every message's key, for duplicates */
    size_t nseen, capseen;
    bool has_header;
    unsigned long nplurals;     /* 0: none */
    struct plural *plural;
    bool bad_header;
};

static struct domain *domains;
static int ndomains;

static struct domain *get_domain(const char *name)
{
    if (o_o)
        name = "messages";
    for (int i = 0; i < ndomains; i++)
        if (strcmp(domains[i].name, name) == 0)
            return &domains[i];
    domains = xrealloc(domains, (size_t)(ndomains + 1) * sizeof *domains);
    struct domain *d = &domains[ndomains++];
    memset(d, 0, sizeof *d);
    d->name = xmemdup(name, strlen(name));
    return d;
}

/* ---- the checks of -c ---- */

/* the argument types of the conversions of a printf format: types[pos]
   (1-based positions) as letters; the number of conversions, or -1 when
   the format is not valid */
static int conversions(const char *s, size_t len, char *types, int max, int *npos)
{
    int count = 0, next = 1;
    *npos = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] != '%')
            continue;
        i++;
        if (i < len && s[i] == '%')
            continue;
        count++;
        /* %n$ */
        size_t j = i;
        int pos = 0;
        while (j < len && s[j] >= '0' && s[j] <= '9')
            pos = pos * 10 + (s[j++] - '0');
        if (j < len && s[j] == '$' && pos > 0)
            i = j + 1;
        else
            pos = 0;
        while (i < len && strchr("-+ #0'", s[i]))
            i++;
        /* the width, and the precision: * takes an int */
        for (int part = 0; part < 2; part++) {
            if (part == 1) {
                if (i < len && s[i] == '.')
                    i++;
                else
                    break;
            }
            if (i < len && s[i] == '*') {
                i++;
                size_t k = i;
                int wp = 0;
                while (k < len && s[k] >= '0' && s[k] <= '9')
                    wp = wp * 10 + (s[k++] - '0');
                if (k < len && s[k] == '$' && wp > 0)
                    i = k + 1;
                else
                    wp = next++;
                if (wp < max)
                    types[wp] = 'i';
                if (wp > *npos)
                    *npos = wp;
            } else {
                while (i < len && s[i] >= '0' && s[i] <= '9')
                    i++;
            }
        }
        char lm[3] = "";
        if (i < len && strchr("hljztL", s[i])) {
            lm[0] = s[i++];
            if (i < len && (s[i] == 'h' || s[i] == 'l') && s[i] == lm[0])
                lm[1] = s[i++];
        }
        if (i >= len)
            return -1;
        char c = s[i], t;
        switch (c) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
            t = !lm[0] ? 'i' : lm[0] == 'h' ? (lm[1] ? 'c' : 's') : lm[0] == 'l' ? (lm[1] ? 'q' : 'l')
                : lm[0] == 'j' ? 'j' : lm[0] == 'z' ? 'z' : lm[0] == 't' ? 't' : '?';
            break;
        case 'a': case 'A': case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
            t = lm[0] == 'L' ? 'D' : 'd';
            break;
        case 'c': t = lm[0] == 'l' ? 'w' : 'i'; break;
        case 's': t = lm[0] == 'l' ? 'W' : 'S'; break;
        case 'p': t = 'p'; break;
        case 'n': t = 'n'; break;
        case 'C': t = 'w'; break;
        case 'S': t = 'W'; break;
        default: return -1;
        }
        if (pos == 0)
            pos = next++;
        if (pos < max)
            types[pos] = t;
        if (pos > *npos)
            *npos = pos;
    }
    return count;
}

static bool check_pair(const struct po_msg *m, const struct po_str *id, const struct po_str *str, int form)
{
    bool ok = true;
    char where[64] = "msgstr";
    if (m->indexed)
        snprintf(where, sizeof where, "msgstr[%d]", form);
    const char *idname = form > 0 ? "msgid_plural" : "msgid";
    if (id->len > 0 && str->len > 0) {
        if ((id->s[0] == '\n') != (str->s[0] == '\n')) {
            fprintf(stderr, "msgfmt: %s:%ld: %s and %s: one starts with a newline, the other not\n", m->file,
                    m->line, idname, where);
            ok = false;
        }
        if ((id->s[id->len - 1] == '\n') != (str->s[str->len - 1] == '\n')) {
            fprintf(stderr, "msgfmt: %s:%ld: %s and %s: one ends with a newline, the other not\n", m->file,
                    m->line, idname, where);
            ok = false;
        }
    }
    bool cf = m->cformat > 0 || (m->cformat == 0 && memchr(m->id.s, '%', m->id.len));
    if (cf && str->len > 0) {
        enum { MAXP = 100 };
        char ta[MAXP] = { 0 }, tb[MAXP] = { 0 };
        int pa, pb;
        int na = conversions(id->s, id->len, ta, MAXP, &pa);
        int nb = conversions(str->s, str->len, tb, MAXP, &pb);
        if (na < 0 && m->cformat > 0) {
            fprintf(stderr, "msgfmt: %s:%ld: %s is not a valid format\n", m->file, m->line, idname);
            ok = false;
        } else if (na >= 0 && nb < 0) {
            fprintf(stderr, "msgfmt: %s:%ld: %s is not a valid format\n", m->file, m->line, where);
            ok = false;
        } else if (na >= 0 && na != nb) {
            fprintf(stderr, "msgfmt: %s:%ld: %s and %s have %d and %d conversions\n", m->file, m->line, idname,
                    where, na, nb);
            ok = false;
        } else if (na >= 0 && (pa != pb || memcmp(ta, tb, MAXP) != 0)) {
            fprintf(stderr, "msgfmt: %s:%ld: %s and %s have conversions for different types\n", m->file,
                    m->line, idname, where);
            ok = false;
        }
    }
    return ok;
}

/* ---- reading ---- */

static void take(const struct po_msg *m, const char *dname, void *arg)
{
    (void)arg;
    if (!m)
        return;                 /* a domain directive: the next messages say which */
    struct domain *d = get_domain(dname);
    bool header = m->id.len == 0 && !m->ctxt.s;
    if (header) {
        if (d->has_header) {
            fprintf(stderr, "msgfmt: %s:%ld: a second header for domain %s\n", m->file, m->line, d->name);
            errors++;
            return;
        }
        d->has_header = true;
        if (m->indexed) {
            fprintf(stderr, "msgfmt: %s:%ld: a header with plural forms\n", m->file, m->line);
            errors++;
            return;
        }
        const char *h = m->strs[0].s;
        int r = plural_header(h, &d->nplurals, &d->plural);
        if (r < 0) {
            fprintf(stderr, "msgfmt: %s:%ld: the plural forms of the header are not valid\n", m->file, m->line);
            errors++;
            d->bad_header = true;
        }
    } else if (m->indexed) {
        if (!d->nplurals && !d->bad_header) {
            fprintf(stderr, "msgfmt: %s:%ld: plural forms, but no nplurals in the header of domain %s\n", m->file,
                    m->line, d->name);
            errors++;
        } else if (d->nplurals && (unsigned long)m->nstrs != d->nplurals) {
            fprintf(stderr, "msgfmt: %s:%ld: %d plural forms, but nplurals=%lu\n", m->file, m->line, m->nstrs,
                    d->nplurals);
            errors++;
        }
    }
    bool empty = m->strs[0].len == 0;
    if (!header) {
        if (m->fuzzy)
            n_fuzzy++;
        else if (empty)
            n_untranslated++;
        else
            n_translated++;
    }
    if (o_c && !header && !empty && (!m->fuzzy || o_f)) {
        for (int i = 0; i < m->nstrs; i++)
            if (!check_pair(m, i > 0 ? &m->plural : &m->id, &m->strs[i], i))
                errors++;
    }
    /* the entry */
    size_t cl = m->ctxt.s ? m->ctxt.len + 1 : 0;
    size_t kl = cl + m->id.len;
    size_t ol = kl + (m->plural.s ? 1 + m->plural.len : 0);
    char *key = xmalloc(ol + 1);
    if (m->ctxt.s) {
        memcpy(key, m->ctxt.s, m->ctxt.len);
        key[m->ctxt.len] = '\004';
    }
    memcpy(key + cl, m->id.s, m->id.len);
    if (m->plural.s) {
        key[kl] = '\0';
        memcpy(key + kl + 1, m->plural.s, m->plural.len);
    }
    key[ol] = '\0';
    /* defined twice: also when one of them is left out */
    for (size_t i = 0; i < d->nseen; i++)
        if (d->seen[i].keylen == kl && memcmp(d->seen[i].key, key, kl) == 0) {
            fprintf(stderr, "msgfmt: %s:%ld: a message defined twice (first at %s:%ld)\n", m->file, m->line,
                    d->seen[i].file, d->seen[i].line);
            errors++;
            free(key);
            return;
        }
    if (d->nseen == d->capseen) {
        d->capseen = d->capseen ? d->capseen * 2 : 64;
        d->seen = xrealloc(d->seen, d->capseen * sizeof *d->seen);
    }
    d->seen[d->nseen++] = (struct entry){ xmemdup(key, kl), kl, kl, NULL, 0, m->file, m->line };
    if (empty || (m->fuzzy && !o_f && !header)) {
        free(key);
        return;
    }
    size_t tl = 0;
    for (int i = 0; i < m->nstrs; i++)
        tl += m->strs[i].len + 1;
    char *trans = xmalloc(tl);
    size_t k = 0;
    for (int i = 0; i < m->nstrs; i++) {
        memcpy(trans + k, m->strs[i].s, m->strs[i].len);
        k += m->strs[i].len;
        trans[k++] = '\0';
    }
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 64;
        d->e = xrealloc(d->e, d->cap * sizeof *d->e);
    }
    d->e[d->n++] = (struct entry){ key, kl, ol, trans, tl - 1, m->file, m->line };
}

/* ---- writing ---- */

static unsigned long hash_string(const char *s)
{
    unsigned long h = 0, g;
    while (*s) {
        h = (h << 4) + (unsigned char)*s++;
        g = h & 0xf0000000ul;
        if (g) {
            h ^= g >> 24;
            h ^= g;
        }
        h &= 0xfffffffful;
    }
    return h;
}

/* the primality test of GNU gettext's hash table sizes, as it is (it
   takes 3 for no prime), so that the tables are the same size */
static bool is_prime(unsigned long n)
{
    unsigned long d = 3, sq = d * d;
    while (sq < n && n % d != 0) {
        d++;
        sq += 4 * d;
        d++;
    }
    return n % d != 0;
}

static unsigned long next_prime(unsigned long n)
{
    n |= 1;
    while (!is_prime(n))
        n += 2;
    return n;
}

/* compare the sort keys: the bytes of msgid (with its context) */
static int by_key(const void *a, const void *b)
{
    const struct entry *x = a, *y = b;
    size_t n = x->keylen < y->keylen ? x->keylen : y->keylen;
    int c = memcmp(x->key, y->key, n);
    if (c)
        return c;
    return x->keylen < y->keylen ? -1 : x->keylen > y->keylen;
}

static void put32(unsigned char *p, uint32_t v)
{
    memcpy(p, &v, 4);           /* the byte order of this machine */
}

static bool write_mo(struct domain *d, const char *path)
{
    if (d->n > 1)
        qsort(d->e, d->n, sizeof *d->e, by_key);
    size_t n = d->n;
    unsigned long hsize = next_prime((unsigned long)n * 4 / 3);
    if (hsize <= 2)
        hsize = 3;
    size_t head = 28, tabs = head + 16 * n, strs = tabs + 4 * hsize;
    size_t total = strs;
    for (size_t i = 0; i < n; i++)
        total += d->e[i].origlen + 1 + d->e[i].translen + 1;
    if (total > 0xffffffffu) {
        fprintf(stderr, "msgfmt: %s: too large\n", path);
        return false;
    }
    unsigned char *buf = calloc(1, total);
    if (!buf) {
        fputs("msgfmt: out of memory\n", stderr);
        exit(1);
    }
    put32(buf, 0x950412de);
    put32(buf + 4, 0);
    put32(buf + 8, (uint32_t)n);
    put32(buf + 12, (uint32_t)head);
    put32(buf + 16, (uint32_t)(head + 8 * n));
    put32(buf + 20, (uint32_t)hsize);
    put32(buf + 24, (uint32_t)tabs);
    size_t off = strs;
    for (size_t i = 0; i < n; i++) {
        put32(buf + head + 8 * i, (uint32_t)d->e[i].origlen);
        put32(buf + head + 8 * i + 4, (uint32_t)off);
        memcpy(buf + off, d->e[i].key, d->e[i].origlen);
        off += d->e[i].origlen + 1;
    }
    for (size_t i = 0; i < n; i++) {
        put32(buf + head + 8 * n + 8 * i, (uint32_t)d->e[i].translen);
        put32(buf + head + 8 * n + 8 * i + 4, (uint32_t)off);
        memcpy(buf + off, d->e[i].trans, d->e[i].translen);
        off += d->e[i].translen + 1;
    }
    /* the hash table: open addressing with double hashing, as GNU */
    uint32_t *ht = calloc(hsize, sizeof *ht);
    if (!ht) {
        fputs("msgfmt: out of memory\n", stderr);
        exit(1);
    }
    for (size_t i = 0; i < n; i++) {
        char *k = xmemdup(d->e[i].key, d->e[i].keylen);
        unsigned long hv = hash_string(k);
        free(k);
        unsigned long idx = hv % hsize, incr = 1 + hv % (hsize - 2);
        while (ht[idx] != 0)
            idx = idx >= hsize - incr ? idx - (hsize - incr) : idx + incr;
        ht[idx] = (uint32_t)(i + 1);
    }
    for (unsigned long i = 0; i < hsize; i++)
        put32(buf + tabs + 4 * i, ht[i]);
    free(ht);

    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(buf, 1, total, f) == total;
    if (f && fclose(f) != 0)
        ok = false;
    if (!ok)
        fprintf(stderr, "msgfmt: %s: %s\n", path, strerror(errno));
    free(buf);
    return ok;
}

static void usage(void)
{
    fputs("usage: msgfmt [-cfSv] [-D dir] [-o outputfile] pathname...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "cfSvD:o:")) != -1) {
        switch (c) {
        case 'c': o_c = true; break;
        case 'f': o_f = true; break;
        case 'S': o_S = true; break;
        case 'v': o_v = true; break;
        case 'D':
            dirs = xrealloc(dirs, (size_t)(ndirs + 1) * sizeof *dirs);
            dirs[ndirs++] = optarg;
            break;
        case 'o': o_o = optarg; break;
        default: usage();
        }
    }
    if (optind >= argc)
        usage();
    bool failed = false;
    for (int i = optind; i < argc; i++) {
        const char *p = argv[i];
        FILE *f;
        char path[PATH_MAX];
        if (strcmp(p, "-") == 0) {
            f = stdin;
            p = "standard input";
        } else {
            f = fopen(p, "r");
            int e = errno;
            for (int k = 0; !f && p[0] != '/' && k < ndirs; k++) {
                if (snprintf(path, sizeof path, "%s/%s", dirs[k], p) < (int)sizeof path && (f = fopen(path, "r")))
                    p = path;
            }
            if (!f) {
                fprintf(stderr, "msgfmt: %s: %s\n", argv[i], strerror(e));
                failed = true;
                continue;
            }
        }
        char *name = xmemdup(p, strlen(p));
        errors += po_read(f, name, "msgfmt", take, NULL);
        if (f != stdin)
            fclose(f);
    }
    if (o_v)
        fprintf(stderr, "msgfmt: %ld translated message%s, %ld fuzzy, %ld untranslated\n", n_translated,
                n_translated == 1 ? "" : "s", n_fuzzy, n_untranslated);
    if (errors || failed)
        return 1;
    for (int i = 0; i < ndomains; i++) {
        char *out;
        if (domains[i].n == 0)
            continue;               /* nothing to write: no file (as GNU msgfmt) */
        if (o_o) {
            size_t l = strlen(o_o);
            bool add = o_S && (l < 3 || strcmp(o_o + l - 3, ".mo") != 0);
            out = xmalloc(l + 4);
            snprintf(out, l + 4, "%s%s", o_o, add ? ".mo" : "");
        } else {
            size_t l = strlen(domains[i].name);
            out = xmalloc(l + 4);
            snprintf(out, l + 4, "%s.mo", domains[i].name);
        }
        if (!write_mo(&domains[i], out))
            failed = true;
        free(out);
    }
    return failed ? 1 : 0;
}
