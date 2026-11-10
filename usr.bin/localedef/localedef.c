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
 * localedef.c - localedef: define locale environment (POSIX.1-2024, XCU
 * localedef)
 *
 * Synopsis: localedef [-c] [-f charmap] [-i sourcefile] [-u code_set_name]
 *                     name
 *
 * The locale definition (from sourcefile, or the standard input) and the
 * charmap (-f; by default ANSI_X3.4-1968) are those of POSIX, with the
 * extensions of glibc (its keywords, the categories LC_PAPER, LC_NAME,
 * LC_ADDRESS, LC_TELEPHONE, LC_MEASUREMENT and LC_IDENTIFICATION, copy,
 * include, translit_start ...); the locale made is in glibc's binary
 * format, the files of the categories, byte for byte as glibc's
 * localedef writes them (lex.c, charmap.c, the categories in simple.c,
 * ctype.c and collate.c).
 *
 * The name: a path name (with a "/") is the directory the files are
 * written to (made if it is not there); another name is the directory of
 * that name in /usr/lib/locale (the codeset after "." normalized:
 * "UTF-8" is "utf8"), not glibc's locale archive.  The files: LC_CTYPE,
 * LC_COLLATE, ..., LC_MESSAGES/SYS_LC_MESSAGES.
 *
 * Locale sources named in "copy" (and the -i file, when it is not
 * there) are looked for in the directories of I18NPATH (DIR/locales,
 * DIR) and in /usr/share/i18n/locales.  -u code_set_name: a repertoire
 * map (in /usr/share/i18n/repertoiremaps).
 *
 * Exit status: 0, no problems; 1, warnings (or, with -c, errors), the
 * locale made; 4, errors, no locale made (2 and 3 are not used); a
 * problem in the syntax of the input is a warning (the rest of the line
 * is skipped, as glibc does; glibc's localedef reports it, but does not
 * count it in its exit status).
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *const category_names[C_LAST] = {
    "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES", "LC_ALL",
    "LC_PAPER", "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION",
};

int nerrors, nwarnings;
bool verbose, quiet;
struct localedef *locales;
const char *repertoire_global;

/* ---- memory, diagnostics ---- */

static void nomem(void)
{
    fputs("localedef: out of memory\n", stderr);
    exit(4);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        nomem();
    return p;
}

void *xcalloc(size_t n, size_t m)
{
    void *p = calloc(n ? n : 1, m ? m : 1);
    if (!p)
        nomem();
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        nomem();
    return p;
}

char *xstrndup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *xstrdup(const char *s)
{
    return xstrndup(s, strlen(s));
}

char *str_sep(char **sp, const char *delim)
{
    char *s = *sp;
    if (!s)
        return NULL;
    char *e = s + strcspn(s, delim);
    if (*e) {
        *e = '\0';
        *sp = e + 1;
    } else {
        *sp = NULL;
    }
    return s;
}

void *xmemdup(const void *p, size_t n)
{
    return memcpy(xmalloc(n), p, n);
}

char *xasprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

void record_error(int status, const char *fmt, ...)
{
    va_list ap;
    nerrors++;
    if (!quiet || status) {
        fputs("localedef: [error] ", stderr);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }
    if (status)
        exit(status);
}

void record_warning(const char *fmt, ...)
{
    va_list ap;
    nwarnings++;
    if (!quiet) {
        fputs("localedef: [warning] ", stderr);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }
}

/* ---- the hash table (glibc's, for its order of iteration) ---- */

uint32_t compute_hashval(const void *key, size_t keylen)
{
    uint32_t h = (uint32_t)keylen;
    for (size_t i = 0; i < keylen; i++) {
        h = (h << 9) | (h >> 23);
        h += ((const unsigned char *)key)[i];
    }
    return h != 0 ? h : ~(uint32_t)0;
}

static int is_prime(unsigned long c)
{
    unsigned long d = 3, sq = d * d;
    while (sq < c && c % d != 0) {
        ++d;
        sq += 4 * d;
        ++d;
    }
    return c % d != 0;
}

unsigned long next_prime(unsigned long seed)
{
    seed |= 1;
    while (!is_prime(seed))
        seed += 2;
    return seed;
}

void ht_init(struct htab *h, unsigned long size)
{
    size = next_prime(size);
    h->size = size;
    h->filled = 0;
    h->first = NULL;
    h->table = xcalloc(size + 1, sizeof *h->table);
}

static size_t ht_lookup(const struct htab *h, const void *key, size_t keylen, unsigned long hval)
{
    struct hentry *t = h->table;
    unsigned long hash = 1 + hval % h->size;
    size_t idx = hash;
    if (t[idx].used) {
        if (t[idx].used == hval && t[idx].keylen == keylen && memcmp(t[idx].key, key, keylen) == 0)
            return idx;
        hash = 1 + hval % (h->size - 2);
        do {
            if (idx <= hash)
                idx = h->size + idx - hash;
            else
                idx -= hash;
            if (t[idx].used == hval && t[idx].keylen == keylen && memcmp(t[idx].key, key, keylen) == 0)
                return idx;
        } while (t[idx].used);
    }
    return idx;
}

static void ht_insert2(struct htab *h, const void *key, size_t keylen, unsigned long hval, size_t idx,
                       void *data)
{
    struct hentry *t = h->table;
    t[idx].used = hval;
    t[idx].key = key;
    t[idx].keylen = keylen;
    t[idx].data = data;
    if (!h->first) {
        t[idx].next = &t[idx];
        h->first = &t[idx];
    } else {
        t[idx].next = h->first->next;
        h->first->next = &t[idx];
        h->first = &t[idx];
    }
    ++h->filled;
    if (100 * h->filled > 75 * h->size) {
        unsigned long old = h->size;
        h->size = next_prime(h->size * 2);
        h->filled = 0;
        h->first = NULL;
        h->table = xcalloc(1 + h->size, sizeof *h->table);
        for (idx = 1; idx <= old; ++idx)
            if (t[idx].used)
                ht_insert2(h, t[idx].key, t[idx].keylen, t[idx].used,
                           ht_lookup(h, t[idx].key, t[idx].keylen, t[idx].used), t[idx].data);
        free(t);
    }
}

int ht_insert(struct htab *h, const void *key, size_t keylen, void *data)
{
    unsigned long hval = compute_hashval(key, keylen);
    size_t idx = ht_lookup(h, key, keylen, hval);
    if (h->table[idx].used)
        return -1;
    ht_insert2(h, xmemdup(key, keylen ? keylen : 1), keylen, hval, idx, data);
    return 0;
}

int ht_find(const struct htab *h, const void *key, size_t keylen, void **data)
{
    size_t idx = ht_lookup(h, key, keylen, compute_hashval(key, keylen));
    if (h->table[idx].used == 0)
        return -1;
    *data = h->table[idx].data;
    return 0;
}

int ht_set(struct htab *h, const void *key, size_t keylen, void *data)
{
    size_t idx = ht_lookup(h, key, keylen, compute_hashval(key, keylen));
    if (h->table[idx].used == 0)
        return -1;
    h->table[idx].data = data;
    return 0;
}

int ht_iterate(const struct htab *h, void **ptr, const void **key, size_t *keylen, void **data)
{
    if (*ptr == NULL) {
        if (!h->first)
            return -1;
        *ptr = h->first->next;
    } else {
        if (*ptr == h->first)
            return -1;
        *ptr = ((struct hentry *)*ptr)->next;
    }
    *key = ((struct hentry *)*ptr)->key;
    *keylen = ((struct hentry *)*ptr)->keylen;
    *data = ((struct hentry *)*ptr)->data;
    return 0;
}

/* ---- reading locale definitions ---- */

static struct lexer *open_source(const char *filename)
{
    struct lexer *lr = lr_open(filename, locfile_keyword);
    if (lr || !filename || filename[0] == '/')
        return lr;
    const char *ip = getenv("I18NPATH");
    if (ip && *ip) {
        char *copy = xstrdup(ip), *save = copy, *next;
        while (!lr && (next = str_sep(&save, ":")) != NULL) {
            char *p = xasprintf("%s/locales/%s", next, filename);
            lr = lr_open(p, locfile_keyword);
            free(p);
            if (!lr) {
                p = xasprintf("%s/%s", next, filename);
                lr = lr_open(p, locfile_keyword);
                free(p);
            }
        }
        free(copy);
    }
    if (!lr) {
        char *p = xasprintf("%s/%s", LOCSRCDIR, filename);
        lr = lr_open(p, locfile_keyword);
        free(p);
    }
    return lr;
}

int locfile_read(struct localedef *result, const struct charmap *cm)
{
    const char *repertoire_name = result->repertoire_name;
    int mask = result->needed & ~result->avail;
    int not_here = ALL_CATEGORIES;
    if (!repertoire_name)
        repertoire_name = repertoire_global;
    struct lexer *lr = open_source(result->name);
    if (!lr)
        return 1;
    for (;;) {
        struct token *now = lr_token(lr, cm, NULL, NULL, verbose);
        enum tok t = now->tok;
        if (t == tok_eof)
            break;
        if (t == tok_eol)
            continue;
        int cat = -1;
        switch (t) {
        case tok_escape_char:
        case tok_comment_char: {
            struct token *arg = lr_token(lr, cm, NULL, NULL, verbose);
            if (arg->tok != tok_ident) {
                SYNTAX_ERROR(lr, "bad argument");
                continue;
            }
            if (arg->str.lenmb != 1) {
                lr_error(lr, "argument to `%s' must be a single character",
                         t == tok_escape_char ? "escape_char" : "comment_char");
                lr_ignore_rest(lr, 0);
                continue;
            }
            if (t == tok_escape_char)
                lr->escape_char = *arg->str.startmb;
            else
                lr->comment_char = *arg->str.startmb;
            break;
        }
        case tok_repertoiremap: {
            struct token *arg = lr_token(lr, cm, NULL, NULL, verbose);
            if (arg->tok != tok_ident) {
                SYNTAX_ERROR(lr, "bad argument");
                continue;
            }
            if (!repertoire_name)
                repertoire_name = xstrndup(arg->str.startmb, arg->str.lenmb);
            break;
        }
        case tok_lc_ctype: cat = C_CTYPE; break;
        case tok_lc_collate: cat = C_COLLATE; break;
        case tok_lc_monetary: cat = C_MONETARY; break;
        case tok_lc_numeric: cat = C_NUMERIC; break;
        case tok_lc_time: cat = C_TIME; break;
        case tok_lc_messages: cat = C_MESSAGES; break;
        case tok_lc_paper: cat = C_PAPER; break;
        case tok_lc_name: cat = C_NAME; break;
        case tok_lc_address: cat = C_ADDRESS; break;
        case tok_lc_telephone: cat = C_TELEPHONE; break;
        case tok_lc_measurement: cat = C_MEASUREMENT; break;
        case tok_lc_identification: cat = C_IDENTIFICATION; break;
        default:
            SYNTAX_ERROR(lr, "syntax error: not inside a locale definition section");
            continue;
        }
        if (cat >= 0) {
            int ignore = (mask & (1 << cat)) == 0;
            switch (cat) {
            case C_CTYPE: ctype_read(lr, result, cm, repertoire_name, ignore); break;
            case C_COLLATE: collate_read(lr, result, cm, repertoire_name, ignore); break;
            case C_MONETARY: monetary_read(lr, result, cm, repertoire_name, ignore); break;
            case C_NUMERIC: numeric_read(lr, result, cm, repertoire_name, ignore); break;
            case C_TIME: time_read(lr, result, cm, repertoire_name, ignore); break;
            case C_MESSAGES: messages_read(lr, result, cm, repertoire_name, ignore); break;
            case C_PAPER: paper_read(lr, result, cm, repertoire_name, ignore); break;
            case C_NAME: name_read(lr, result, cm, repertoire_name, ignore); break;
            case C_ADDRESS: address_read(lr, result, cm, repertoire_name, ignore); break;
            case C_TELEPHONE: telephone_read(lr, result, cm, repertoire_name, ignore); break;
            case C_MEASUREMENT: measurement_read(lr, result, cm, repertoire_name, ignore); break;
            case C_IDENTIFICATION: identification_read(lr, result, cm, repertoire_name, ignore); break;
            }
            result->avail |= mask & (1 << cat);
            not_here ^= 1 << cat;
            continue;
        }
        lr_ignore_rest(lr, 1);
    }
    lr_close(lr);
    result->avail |= not_here;
    return 0;
}

struct localedef *add_to_readlist(int category, const char *name, const char *repertoire_name,
                                  int generate, struct localedef *copy_locale)
{
    struct localedef *runp = locales;
    while (runp && strcmp(name, runp->name) != 0)
        runp = runp->next;
    if (!runp) {
        struct localedef *n = xcalloc(1, sizeof *n);
        n->name = name;
        n->repertoire_name = repertoire_name;
        if (!locales)
            runp = locales = n;
        else {
            runp = locales;
            while (runp->next)
                runp = runp->next;
            runp = runp->next = n;
        }
    }
    if (generate && (runp->needed & (1 << category)) && !(runp->avail & (1 << category)))
        record_error(5, "circular dependencies between locale definitions");
    if (copy_locale) {
        if (runp->categories[category])
            record_error(5, "cannot add already read locale `%s' a second time", name);
        else
            runp->categories[category] = copy_locale->categories[category];
    }
    runp->needed |= 1 << category;
    return runp;
}

struct localedef *find_locale(int category, const char *name, const char *repertoire_name,
                              const struct charmap *cm)
{
    struct localedef *r = add_to_readlist(category, name, repertoire_name, 0, NULL);
    if (!(r->avail & (1 << category)) && locfile_read(r, cm) != 0)
        record_error(4, "cannot open locale definition file `%s': %s", r->name, strerror(errno));
    return r;
}

struct localedef *load_locale(int category, const char *name, const char *repertoire_name,
                              const struct charmap *cm, struct localedef *copy_locale)
{
    struct localedef *r = add_to_readlist(category, name, repertoire_name, 1, copy_locale);
    if (!(r->avail & (1 << category)) && locfile_read(r, cm) != 0)
        record_error(4, "cannot open locale definition file `%s': %s", r->name, strerror(errno));
    return r;
}

void handle_copy(struct lexer *lr, const struct charmap *cm, const char *repertoire_name,
                 struct localedef *result, enum tok token, int category, const char *catname,
                 int ignore_content)
{
    struct token *now = lr_token(lr, cm, result, NULL, verbose);
    bool warned = false;
    if (now->tok != tok_string)
        lr_error(lr, "expecting string argument for `copy'");
    else if (!ignore_content) {
        if (!now->str.startmb)
            lr_error(lr, "locale name should consist only of portable characters");
        else {
            add_to_readlist(category, now->str.startmb, repertoire_name, 1, NULL);
            result->copy_name[category] = now->str.startmb;
        }
    }
    lr_ignore_rest(lr, now->tok == tok_string);
    /* the rest must be "END xxx" */
    while ((now = lr_token(lr, cm, result, NULL, verbose))->tok != tok_end && now->tok != tok_eof) {
        if (!warned && now->tok != tok_eol) {
            lr_error(lr, "no other keyword shall be specified when `copy' is used");
            warned = true;
        }
        lr_ignore_rest(lr, 0);
    }
    if (now->tok != tok_eof) {
        now = lr_token(lr, cm, result, NULL, verbose);
        if (now->tok != token)
            lr_error(lr, "`%1$s' definition does not end with `END %1$s'", catname);
        lr_ignore_rest(lr, now->tok == token);
    } else {
        lr_error(lr, "%s: premature end of file", catname);
    }
}

void *resolve_copy(struct localedef *locale, int category, const struct charmap *cm)
{
    if (!locale->categories[category] && locale->copy_name[category]) {
        struct localedef *from = locale;
        do
            from = find_locale(category, from->copy_name[category], from->repertoire_name, cm);
        while (!from->categories[category] && from->copy_name[category]);
        locale->categories[category] = from->categories[category];
    }
    return locale->categories[category];
}

/* ---- writing the files ---- */

size_t wcslen32(const uint32_t *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void grow(struct locfile *f, size_t n)
{
    if (f->len + n > f->cap) {
        f->cap = (f->len + n) * 2 + 1024;
        f->data = xrealloc(f->data, f->cap);
    }
}

static void put(struct locfile *f, const void *p, size_t n)
{
    if (n == 0)
        return;
    grow(f, n);
    memcpy(f->data + f->len, p, n);
    f->len += n;
}

static void record_offset(struct locfile *f)
{
    if (f->stage < 2) {
        if (f->next_element >= f->n_elements) {
            fputs("localedef: internal error: too many elements\n", stderr);
            exit(4);
        }
        f->offsets[f->next_element++] = (uint32_t)(f->len + (f->n_elements + 2) * 4);
        if (f->stage == 1)
            f->stage = 2;
    }
}

void init_locale_data(struct locfile *f, size_t n)
{
    f->n_elements = n;
    f->next_element = 0;
    f->offsets = xcalloc(n, sizeof *f->offsets);
    f->data = NULL;
    f->len = f->cap = 0;
    f->stage = 0;
}

void align_locale_data(struct locfile *f, size_t boundary)
{
    size_t pad = (boundary - f->len % boundary) % boundary;
    if (pad == 0)
        return;
    grow(f, pad);
    memset(f->data + f->len, 0, pad);
    f->len += pad;
}

void add_locale_empty(struct locfile *f)
{
    record_offset(f);
}

void add_locale_raw_data(struct locfile *f, const void *data, size_t size)
{
    record_offset(f);
    put(f, data, size);
}

void add_locale_string(struct locfile *f, const char *s)
{
    record_offset(f);
    put(f, s, strlen(s) + 1);
}

void add_locale_uint32(struct locfile *f, uint32_t v)
{
    align_locale_data(f, 4);
    record_offset(f);
    put(f, &v, 4);
}

void add_locale_uint32_array(struct locfile *f, const uint32_t *v, size_t n)
{
    align_locale_data(f, 4);
    record_offset(f);
    put(f, v, n * 4);
}

void add_locale_wstring(struct locfile *f, const uint32_t *s)
{
    add_locale_uint32_array(f, s, wcslen32(s) + 1);
}

void add_locale_char(struct locfile *f, char c)
{
    record_offset(f);
    put(f, &c, 1);
}

void start_locale_structure(struct locfile *f)
{
    f->stage = 1;
}

void end_locale_structure(struct locfile *f)
{
    record_offset(f);
    f->stage = 0;
}

void start_locale_prelude(struct locfile *f)
{
    f->stage = 3;
}

void end_locale_prelude(struct locfile *f)
{
    f->stage = 0;
}

static bool write_all(int fd, const void *p, size_t n)
{
    const char *s = p;
    while (n > 0) {
        ssize_t w = write(fd, s, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        s += w;
        n -= (size_t)w;
    }
    return true;
}

void write_locale_data(const char *output_path, int category, struct locfile *f)
{
    const char *name = category_names[category];
    uint32_t header[2];
    static const uint32_t magic_base[] = { 0x20090720, 0x20051014, 0x20031115 };
    uint32_t m = category == C_CTYPE ? magic_base[0] : category == C_COLLATE ? magic_base[1] : magic_base[2];
    header[0] = m ^ (uint32_t)category;
    header[1] = (uint32_t)f->n_elements;
    if (f->next_element != f->n_elements) {
        fprintf(stderr, "localedef: internal error: %s: %zu elements of %zu\n", name, f->next_element,
                f->n_elements);
        exit(4);
    }
    char *fname = xasprintf("%s%s", output_path, name);
    int fd = -2;
    if (category == C_MESSAGES) {
        struct stat st;
        if (stat(fname, &st) < 0) {
            if (mkdir(fname, 0777) >= 0) {
                fd = -1;
                errno = EISDIR;
            }
        } else if (!S_ISREG(st.st_mode)) {
            fd = -1;
            errno = EISDIR;
        }
    }
    if (fd == -2) {
        unlink(fname);
        fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }
    if (fd == -1 && errno == EISDIR) {
        free(fname);
        fname = xasprintf("%s%s/SYS_%s", output_path, name, name);
        unlink(fname);
        fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }
    if (fd < 0) {
        record_error(0, "cannot open output file `%s' for category `%s': %s", fname, name, strerror(errno));
        free(fname);
        return;
    }
    if (!write_all(fd, header, sizeof header) || !write_all(fd, f->offsets, f->n_elements * 4) ||
        !write_all(fd, f->data, f->len))
        record_error(0, "failure while writing data for category `%s': %s", name, strerror(errno));
    if (close(fd) != 0)
        record_error(0, "failure while writing data for category `%s': %s", name, strerror(errno));
    free(fname);
    free(f->offsets);
    free(f->data);
}

/* ---- main ---- */

static char *normalize_codeset(const char *cs, size_t n)
{
    bool only_digit = true;
    size_t len = 0;
    for (size_t i = 0; i < n; i++)
        if (isalnum((unsigned char)cs[i])) {
            len++;
            if (isalpha((unsigned char)cs[i]))
                only_digit = false;
        }
    char *r = xmalloc(len + 4), *w = r;
    if (only_digit)
        w += sprintf(w, "iso");
    for (size_t i = 0; i < n; i++)
        if (isalpha((unsigned char)cs[i]))
            *w++ = (char)tolower((unsigned char)cs[i]);
        else if (isdigit((unsigned char)cs[i]))
            *w++ = cs[i];
    *w = '\0';
    return r;
}

static char *output_path_of(const char *name)
{
    if (strchr(name, '/'))
        return xasprintf("%s/", name);
    const char *p = name;
    while (*p && *p != '@' && *p != '.')
        p++;
    if (*p == '.') {
        const char *s = p + 1, *e = s;
        while (*e && *e != '@')
            e++;
        if (e > s) {
            char *n = normalize_codeset(s, (size_t)(e - s));
            char *r = xasprintf("/usr/lib/locale/%.*s%s%s/", (int)(s - name), name, n, e);
            free(n);
            return r;
        }
    }
    return xasprintf("/usr/lib/locale/%s/", name);
}

static void usage(void)
{
    fputs("usage: localedef [-c] [-f charmap] [-i sourcefile] [-u code_set_name] name\n", stderr);
    exit(4);
}

int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE, "");
    setlocale(LC_MESSAGES, "");
    bool force = false;
    const char *charmap_file = NULL, *input = NULL;
    int c;
    while ((c = getopt(argc, argv, "cf:i:u:")) != -1) {
        switch (c) {
        case 'c': force = true; break;
        case 'f': charmap_file = optarg; break;
        case 'i': input = optarg; break;
        case 'u': repertoire_global = optarg; break;
        default: usage();
        }
    }
    if (argc - optind != 1)
        usage();
    if (getenv("POSIXLY_CORRECT"))
        verbose = true;
    char *out = output_path_of(argv[optind]);
    int cannot_write = 0;
    if (access(out, W_OK) != 0) {
        if (errno == ENOENT) {
            if (mkdir(out, 0777) < 0)
                cannot_write = errno;
        } else {
            cannot_write = errno;
        }
    }
    setlocale(LC_CTYPE, "POSIX");
    struct charmap *cm = charmap_read(charmap_file ? charmap_file : DEFAULT_CHARMAP);
    struct localedef global = { 0 };
    global.name = input ? input : "/dev/stdin";
    global.needed = ALL_CATEGORIES;
    locales = &global;
    if (locfile_read(&global, cm) != 0)
        record_error(4, "cannot open locale definition file `%s': %s", global.name, strerror(errno));
    for (;;) {
        struct localedef *r = locales;
        while (r && (r->needed & r->avail) == r->needed)
            r = r->next;
        if (!r)
            break;
        if (locfile_read(r, cm) != 0)
            record_error(4, "cannot open locale definition file `%s': %s", r->name, strerror(errno));
    }
    /* in the order of the categories' numbers, as glibc */
    ctype_finish(&global, cm);
    numeric_finish(&global, cm);
    time_finish(&global, cm);
    collate_finish(&global, cm);
    monetary_finish(&global, cm);
    messages_finish(&global, cm);
    paper_finish(&global, cm);
    name_finish(&global, cm);
    address_finish(&global, cm);
    telephone_finish(&global, cm);
    measurement_finish(&global, cm);
    identification_finish(&global, cm);
    if (nerrors == 0 || force) {
        if (cannot_write) {
            record_error(4, "cannot write output files to `%s': %s", out, strerror(cannot_write));
        }
        int before = nerrors;
        ctype_output(&global, cm, out);
        numeric_output(&global, cm, out);
        time_output(&global, cm, out);
        collate_output(&global, cm, out);
        monetary_output(&global, cm, out);
        messages_output(&global, cm, out);
        paper_output(&global, cm, out);
        name_output(&global, cm, out);
        address_output(&global, cm, out);
        telephone_output(&global, cm, out);
        measurement_output(&global, cm, out);
        identification_output(&global, cm, out);
        if (nerrors > before)
            return 4;
        return nwarnings || nerrors ? 1 : 0;
    }
    record_error(0, "no output file produced because errors were issued");
    return 4;
}
