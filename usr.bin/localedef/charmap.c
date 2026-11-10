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
 * charmap.c - the charmap (and repertoire map) of localedef
 *
 * A charmap is found as named, else (a name without "/") in the
 * directories of I18NPATH (DIR/charmaps, DIR) and CHARMAP_PATH, as it
 * is or compressed (NAME.gz, NAME.bz2: read through gzip -dc, bzip2
 * -dc), else by the <code_set_name> or alias in a file of CHARMAP_PATH;
 * without -f, ANSI_X3.4-1968.  The format is that of POSIX (and glibc):
 * the declarations, CHARMAP (names, ranges <a>..<b>, encodings), END
 * CHARMAP, then WIDTH, WIDTH_VARIABLE and WIDTH_DEFAULT.  A name or an
 * encoding defined twice keeps its first definition.
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

bool enc_not_ascii_compatible;

/* ---- files, maybe compressed ---- */

static FILE *uncompress(const char *path, const char *prog)
{
    int p[2];
    if (access(path, R_OK) != 0 || pipe(p) != 0)
        return NULL;
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        return NULL;
    }
    if (pid == 0) {
        close(p[0]);
        dup2(p[1], 1);
        close(p[1]);
        execlp(prog, prog, "-dc", path, (char *)NULL);
        _exit(127);
    }
    close(p[1]);
    /* the child is waited for when the program exits (it is short-lived) */
    return fdopen(p[0], "r");
}

FILE *open_maybe_compressed(const char *path)
{
    size_t len = strlen(path);
    if (len > 3 && strcmp(path + len - 3, ".gz") == 0)
        return uncompress(path, "gzip");
    if (len > 4 && strcmp(path + len - 4, ".bz2") == 0)
        return uncompress(path, "bzip2");
    FILE *f = fopen(path, "r");
    if (f)
        return f;
    char *p = xasprintf("%s.gz", path);
    f = uncompress(p, "gzip");
    free(p);
    if (f)
        return f;
    p = xasprintf("%s.bz2", path);
    f = uncompress(p, "bzip2");
    free(p);
    return f;
}

static struct lexer *cm_open(const char *dir, const char *name)
{
    size_t dl = strlen(dir);
    char *path = xasprintf("%s%s%s", dir, dl == 0 || dir[dl - 1] != '/' ? "/" : "", name);
    FILE *fp = open_maybe_compressed(path);
    struct lexer *lr = fp ? lr_create(fp, path, charmap_keyword) : NULL;
    free(path);
    return lr;
}

/* ---- the table ---- */

struct charseq *charmap_find_value(const struct charmap *cm, const char *name, size_t len)
{
    void *r;
    return ht_find(&cm->char_table, name, len, &r) < 0 ? NULL : r;
}

struct charseq *charmap_find_symbol(const struct charmap *cm, const char *bytes, size_t n)
{
    void *r;
    return ht_find(&cm->byte_table, bytes, n, &r) < 0 ? NULL : r;
}

static uint32_t name_ucs4(const char *name, size_t len)
{
    if ((name[0] == 'U' || name[0] == 'P') && (len == 5 || len == 9)) {
        char *end;
        errno = 0;
        unsigned long v = strtoul(name + 1, &end, 16);
        if ((size_t)(end - name) != len || (v == ULONG_MAX && errno == ERANGE) || v >= 0x80000000)
            return UNINITIALIZED_CHAR_VALUE;
        return (uint32_t)v;
    }
    return UNINITIALIZED_CHAR_VALUE;
}

static void new_char(struct lexer *lr, struct charmap *cm, size_t nbytes, unsigned char *bytes,
                     const char *from, const char *to, int decimal_ellipsis, int step)
{
    size_t len1 = strlen(from);
    if (!to) {
        struct charseq *s = xmalloc(sizeof *s + nbytes);
        s->nbytes = (int)nbytes;
        memcpy(s->bytes, bytes, nbytes);
        s->name = from;
        s->ucs4 = name_ucs4(from, len1);
        ht_insert(&cm->char_table, from, len1, s);
        ht_insert(&cm->byte_table, s->bytes, nbytes, s);
        return;
    }
    size_t len2 = strlen(to);
    if (len1 != len2) {
    illegal:
        lr_error(lr, "invalid names for character range");
        return;
    }
    const char *cp = &from[len1 - 1];
    if (decimal_ellipsis) {
        while (cp >= from && isdigit((unsigned char)*cp))
            --cp;
    } else {
        while (cp >= from && isxdigit((unsigned char)*cp)) {
            if (!isdigit((unsigned char)*cp) && !isupper((unsigned char)*cp))
                lr_error(lr, "hexadecimal range format should use only capital characters");
            --cp;
        }
    }
    int prefix = (int)(cp - from) + 1;
    if (cp == &from[len1 - 1] || strncmp(from, to, (size_t)prefix) != 0)
        goto illegal;
    char *e1, *e2;
    errno = 0;
    unsigned long fnr = strtoul(&from[prefix], &e1, decimal_ellipsis ? 10 : 16);
    unsigned long tnr = strtoul(&to[prefix], &e2, decimal_ellipsis ? 10 : 16);
    if (*e1 || *e2 || errno == ERANGE) {
        lr_error(lr, "<%s> and <%s> are invalid names for range", from, to);
        return;
    }
    if (fnr > tnr) {
        lr_error(lr, "upper limit in range is smaller than lower limit");
        return;
    }
    for (unsigned long cnt = fnr; cnt <= tnr; cnt += (unsigned long)step) {
        char *name = xasprintf(decimal_ellipsis ? "%.*s%0*lu" : "%.*s%0*lX", prefix, from,
                               (int)len1 - prefix, cnt);
        struct charseq *s = xmalloc(sizeof *s + nbytes);
        s->nbytes = (int)nbytes;
        memcpy(s->bytes, bytes, nbytes);
        s->name = name;
        s->ucs4 = name_ucs4(name, len1);
        ht_insert(&cm->char_table, name, len1, s);
        ht_insert(&cm->byte_table, s->bytes, nbytes, s);
        if (++bytes[nbytes - 1] == 0) {
            int b = (int)nbytes - 2;
            do {
                if (b < 0) {
                    lr_error(lr, "resulting bytes for range not representable.");
                    return;
                }
            } while (++bytes[b--] == 0);
        }
    }
}

static void new_width(struct lexer *lr, struct charmap *cm, const char *from, const char *to,
                      unsigned long width)
{
    struct charseq *fv = charmap_find_value(cm, from, strlen(from)), *tv;
    if (!fv) {
        lr_error(lr, "unknown character `%s'", from);
        return;
    }
    if (!to) {
        tv = fv;
    } else {
        tv = charmap_find_value(cm, to, strlen(to));
        if (!tv) {
            lr_error(lr, "unknown character `%s'", to);
            return;
        }
        if (fv->nbytes != tv->nbytes) {
            lr_error(lr, "number of bytes for byte sequence of beginning and end of range not the same: %d vs %d",
                     fv->nbytes, tv->nbytes);
            return;
        }
    }
    if (cm->nwidth_rules >= cm->nwidth_rules_max) {
        cm->nwidth_rules_max = cm->nwidth_rules + 32;
        cm->width_rules = xrealloc(cm->width_rules, cm->nwidth_rules_max * sizeof *cm->width_rules);
    }
    cm->width_rules[cm->nwidth_rules++] = (struct width_rule){ fv, tv, (unsigned)width };
}

static char *sym_name(struct token *t)
{
    if (t->tok == tok_bsymbol)
        return xstrndup(t->str.startmb, t->str.lenmb);
    return xasprintf("U%08X", t->ucs4);
}

static struct charmap *parse_charmap(struct lexer *lr)
{
    struct charmap *cm = xcalloc(1, sizeof *cm);
    lr->translate_strings = 0;
    cm->width_default = 1;
    ht_init(&cm->char_table, 256);
    ht_init(&cm->byte_table, 256);
    ht_init(&cm->ucs4_table, 256);
    int state = 1, step = 1;
    enum tok expected = tok_error, ellipsis = tok_none;
    const char *expected_str = NULL;
    char *from = NULL, *to = NULL;
    for (;;) {
        struct token *now = lr_token(lr, NULL, NULL, NULL, verbose);
        enum tok t = now->tok;
        if (t == tok_eof)
            break;
        switch (state) {
        case 1:
            if (t == tok_eol)
                continue;
            if (t == tok_charmap) {
                from = to = NULL;
                if (cm->mb_cur_max == 0)
                    cm->mb_cur_max = 1;
                if (cm->mb_cur_min == 0)
                    cm->mb_cur_min = cm->mb_cur_max;
                if (cm->mb_cur_min > cm->mb_cur_max) {
                    record_error(0, "%s: <mb_cur_max> must be greater than <mb_cur_min>", lr->fname);
                    cm->mb_cur_min = cm->mb_cur_max;
                }
                lr_ignore_rest(lr, 1);
                state = 2;
                continue;
            }
            if (t != tok_code_set_name && t != tok_mb_cur_max && t != tok_mb_cur_min &&
                t != tok_escape_char && t != tok_comment_char && t != tok_g0esc && t != tok_g1esc &&
                t != tok_g2esc && t != tok_g3esc && t != tok_repertoiremap && t != tok_include) {
                lr_error(lr, "syntax error in prolog: %s", "invalid definition");
                lr_ignore_rest(lr, 0);
                continue;
            }
            {
                struct token *arg = lr_token(lr, NULL, NULL, NULL, verbose);
                switch (t) {
                case tok_code_set_name:
                case tok_repertoiremap:
                    if (arg->tok != tok_ident && arg->tok != tok_string) {
                    badarg:
                        lr_error(lr, "syntax error in prolog: %s", "bad argument");
                        lr_ignore_rest(lr, 0);
                        continue;
                    }
                    if (t == tok_code_set_name)
                        cm->code_set_name = xstrndup(arg->str.startmb, arg->str.lenmb);
                    else
                        cm->repertoiremap = xstrndup(arg->str.startmb, arg->str.lenmb);
                    lr_ignore_rest(lr, 1);
                    continue;
                case tok_mb_cur_max:
                case tok_mb_cur_min:
                    if (arg->tok != tok_number)
                        goto badarg;
                    if (arg->num < 1) {
                        lr_error(lr, "value for <%s> must be 1 or greater",
                                 t == tok_mb_cur_min ? "mb_cur_min" : "mb_cur_max");
                        lr_ignore_rest(lr, 0);
                        continue;
                    }
                    if ((t == tok_mb_cur_max && cm->mb_cur_min != 0 && (int)arg->num < cm->mb_cur_min) ||
                        (t == tok_mb_cur_min && cm->mb_cur_max != 0 && (int)arg->num > cm->mb_cur_max)) {
                        lr_error(lr, "value of <%s> must be greater or equal than the value of <%s>",
                                 "mb_cur_max", "mb_cur_min");
                        lr_ignore_rest(lr, 0);
                        continue;
                    }
                    if (t == tok_mb_cur_max)
                        cm->mb_cur_max = (int)arg->num;
                    else
                        cm->mb_cur_min = (int)arg->num;
                    lr_ignore_rest(lr, 1);
                    continue;
                case tok_escape_char:
                case tok_comment_char:
                    if (arg->tok != tok_ident)
                        goto badarg;
                    if (arg->str.lenmb != 1) {
                        lr_error(lr, "argument to <%s> must be a single character",
                                 t == tok_escape_char ? "escape_char" : "comment_char");
                        lr_ignore_rest(lr, 0);
                        continue;
                    }
                    if (t == tok_escape_char)
                        lr->escape_char = *arg->str.startmb;
                    else
                        lr->comment_char = *arg->str.startmb;
                    lr_ignore_rest(lr, 1);
                    continue;
                case tok_include:
                    lr_error(lr, "character sets with locking states are not supported");
                    exit(4);
                default:
                    lr_ignore_rest(lr, 0);
                    continue;
                }
            }
        case 2:
            if (t == tok_eol)
                continue;
            if (t == tok_end) {
                expected = tok_charmap;
                expected_str = "CHARMAP";
                state = 90;
                continue;
            }
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "CHARMAP", "no symbolic name given");
                lr_ignore_rest(lr, 0);
                continue;
            }
            free(from);
            from = sym_name(now);
            to = NULL;
            state = 3;
            continue;
        case 3:
            if (t == tok_ellipsis3 || t == tok_ellipsis4 || t == tok_ellipsis2 || t == tok_ellipsis4_2 ||
                t == tok_ellipsis2_2) {
                ellipsis = t;
                if (t == tok_ellipsis4_2 || t == tok_ellipsis2_2)
                    step = 2;
                state = 4;
                continue;
            }
            /* fall through */
        case 5:
            if (t != tok_charcode) {
                lr_error(lr, "syntax error in %s definition: %s", "CHARMAP", "invalid encoding given");
                lr_ignore_rest(lr, 0);
                state = 2;
                continue;
            }
            if (now->charcode.nbytes < cm->mb_cur_min)
                lr_error(lr, "too few bytes in character encoding");
            else if (now->charcode.nbytes > cm->mb_cur_max)
                lr_error(lr, "too many bytes in character encoding");
            else {
                new_char(lr, cm, (size_t)now->charcode.nbytes, now->charcode.bytes, from, to,
                         ellipsis != tok_ellipsis2, step);
                from = NULL;    /* kept by the table */
            }
            lr_ignore_rest(lr, 0);
            free(from);
            from = to = NULL;
            ellipsis = tok_none;
            step = 1;
            state = 2;
            continue;
        case 4:
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "CHARMAP",
                         "no symbolic name given for end of range");
                lr_ignore_rest(lr, 0);
                continue;
            }
            to = sym_name(now);
            state = 5;
            continue;
        case 90:
            if (t != expected)
                lr_error(lr, "%s: definition does not end with `END %s'", expected_str, expected_str);
            lr_ignore_rest(lr, t == expected);
            state = 91;
            continue;
        case 91:
            if (t == tok_eol)
                continue;
            if (t == tok_width_default) {
                state = 92;
                continue;
            }
            if (t == tok_width) {
                lr_ignore_rest(lr, 1);
                state = 93;
                continue;
            }
            if (t == tok_width_variable) {
                lr_ignore_rest(lr, 1);
                state = 98;
                continue;
            }
            lr_error(lr, "only WIDTH definitions are allowed to follow the CHARMAP definition");
            lr_ignore_rest(lr, 0);
            continue;
        case 92:
            if (t != tok_number)
                lr_error(lr, "value for %s must be an integer", "WIDTH_DEFAULT");
            else
                cm->width_default = (unsigned)now->num;
            lr_ignore_rest(lr, t == tok_number);
            state = 91;
            continue;
        case 93:
            if (t == tok_eol)
                continue;
            if (t == tok_end) {
                expected = tok_width;
                expected_str = "WIDTH";
                state = 90;
                continue;
            }
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "WIDTH", "no symbolic name given");
                lr_ignore_rest(lr, 0);
                continue;
            }
            free(from);
            from = sym_name(now);
            to = NULL;
            state = 94;
            continue;
        case 94:
            if (t == tok_ellipsis3) {
                state = 95;
                continue;
            }
            /* fall through */
        case 96:
            if (t != tok_number)
                lr_error(lr, "value for %s must be an integer", "WIDTH");
            else {
                new_width(lr, cm, from, to, now->num);
                free(from);
                free(to);
                from = to = NULL;
            }
            lr_ignore_rest(lr, t == tok_number);
            state = 93;
            continue;
        case 95:
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "WIDTH",
                         "no symbolic name given for end of range");
                lr_ignore_rest(lr, 0);
                state = 93;
                continue;
            }
            to = sym_name(now);
            state = 96;
            continue;
        case 98:
            if (t == tok_eol)
                continue;
            if (t == tok_end) {
                expected = tok_width_variable;
                expected_str = "WIDTH_VARIABLE";
                state = 90;
                continue;
            }
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "WIDTH_VARIABLE", "no symbolic name given");
                lr_ignore_rest(lr, 0);
                continue;
            }
            state = 99;
            continue;
        case 99:
            if (t == tok_ellipsis3)
                state = 100;
            else
                state = 98;
            continue;
        case 100:
            if (t != tok_bsymbol && t != tok_ucs4) {
                lr_error(lr, "syntax error in %s definition: %s", "WIDTH_VARIABLE",
                         "no symbolic name given for end of range");
                lr_ignore_rest(lr, 0);
                continue;
            }
            lr_ignore_rest(lr, 1);
            state = 98;
            continue;
        }
    }
    if (state != 91)
        record_error(0, "%s: premature end of file", lr->fname);
    lr_close(lr);
    return cm;
}

/* the names a charmap file goes by: <code_set_name> and "% alias" lines */
static char **aliases_of(const char *dir, const char *name)
{
    char **v = xcalloc(1, sizeof *v);
    size_t n = 0;
    char *path = xasprintf("%s/%s", dir, name);
    FILE *f = open_maybe_compressed(path);
    free(path);
    if (!f)
        return v;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, f)) >= 0) {
        char *p = line, *e;
        while (isspace((unsigned char)*p))
            p++;
        if (strncmp(p, "CHARMAP", 7) == 0)
            break;
        if (strncmp(p, "<code_set_name>", 15) == 0)
            p += 15;
        else if (strncmp(p, "% alias", 7) == 0)
            p += 7;
        else
            continue;
        while (isspace((unsigned char)*p))
            p++;
        for (e = p; *e && !isspace((unsigned char)*e); e++)
            ;
        if (e > p) {
            v = xrealloc(v, (n + 2) * sizeof *v);
            v[n++] = xstrndup(p, (size_t)(e - p));
            v[n] = NULL;
        }
    }
    free(line);
    /* the rest of the file is not needed */
    fclose(f);
    return v;
}

struct charmap *charmap_read(const char *filename)
{
    struct charmap *cm = NULL;
    if (filename) {
        struct lexer *lr = NULL;
        FILE *fp = strchr(filename, '/') ? open_maybe_compressed(filename) : fopen(filename, "r");
        if (fp)
            lr = lr_create(fp, filename, charmap_keyword);
        if (!lr && !strchr(filename, '/')) {
            const char *ip = getenv("I18NPATH");
            if (ip && *ip) {
                char *copy = xstrdup(ip), *save = copy, *next;
                while (!lr && (next = str_sep(&save, ":")) != NULL) {
                    char *d = xasprintf("%s/charmaps", next);
                    lr = cm_open(d, filename);
                    free(d);
                    if (!lr)
                        lr = cm_open(next, filename);
                }
                free(copy);
            }
            if (!lr)
                lr = cm_open(CHARMAP_PATH, filename);
        }
        if (lr)
            cm = parse_charmap(lr);
        if (!cm && strchr(filename, '/')) {
            record_error(0, "character map file `%s' not found: %s", filename, strerror(errno));
        }
    }
    if (!cm && filename && !strchr(filename, '/')) {
        DIR *d = opendir(CHARMAP_PATH);
        struct dirent *e;
        while (d && !cm && (e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.')
                continue;
            char **al = aliases_of(CHARMAP_PATH, e->d_name);
            bool found = false;
            for (char **p = al; *p; p++)
                if (strcasecmp(*p, filename) == 0)
                    found = true;
            for (char **p = al; *p; p++)
                free(*p);
            free(al);
            if (found) {
                struct lexer *lr = cm_open(CHARMAP_PATH, e->d_name);
                if (lr)
                    cm = parse_charmap(lr);
                break;
            }
        }
        if (d)
            closedir(d);
        if (!cm)
            record_error(0, "character map file `%s' not found", filename);
    }
    if (!cm) {
        struct lexer *lr = cm_open(CHARMAP_PATH, DEFAULT_CHARMAP);
        if (lr)
            cm = parse_charmap(lr);
        if (!cm)
            record_error(4, "default character map file `%s' not found", DEFAULT_CHARMAP);
    }
    if (cm && !cm->code_set_name) {
        const char *b = filename ? strrchr(filename, '/') : NULL;
        cm->code_set_name = xstrdup(b ? b + 1 : filename ? filename : DEFAULT_CHARMAP);
    }
    if (cm) {
        static const char basic[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                                    "!\"#%&'()*+,-./:;<=>?[\\]^_{|}~ \t\v\f";
        bool failed = false;
        for (size_t i = 0; i <= sizeof basic - 1; i++) {     /* with the NUL */
            struct charseq *s = charmap_find_symbol(cm, &basic[i], 1);
            if (!s || s->ucs4 != (uint32_t)(unsigned char)basic[i])
                failed = true;
        }
        if (failed) {
            record_warning("character map `%s' is not ASCII compatible, locale not ISO C compliant",
                           cm->code_set_name);
            enc_not_ascii_compatible = true;
        }
    }
    return cm;
}

/* ---- repertoire maps ---- */

uint32_t repertoire_find_value(const struct repertoire *rep, const char *name, size_t len)
{
    void *r;
    if (!rep || ht_find(&rep->char_table, name, len, &r) < 0)
        return ILLEGAL_CHAR_VALUE;
    return (uint32_t)(uintptr_t)r;
}

const char *repertoire_find_symbol(const struct repertoire *rep, uint32_t ucs)
{
    void *r;
    if (!rep || ht_find(&rep->reverse_table, &ucs, sizeof ucs, &r) < 0)
        return NULL;
    return r;
}

struct charseq *repertoire_find_seq(const struct repertoire *rep, uint32_t ucs)
{
    void *r;
    if (!rep || ht_find(&rep->seq_table, &ucs, sizeof ucs, &r) < 0)
        return NULL;
    return r;
}

/* a repertoire map: lines "<name> <Uxxxx> comment" between
   CHARIDS and END CHARIDS (or all lines) */
struct repertoire *repertoire_read(const char *name)
{
    static struct repertoire *cache;
    static char *cache_name;
    if (cache_name && strcmp(cache_name, name) == 0)
        return cache;
    FILE *f = NULL;
    if (strchr(name, '/'))
        f = open_maybe_compressed(name);
    else {
        char *p = xasprintf("%s/%s", REPERTOIREMAP_PATH, name);
        f = open_maybe_compressed(p);
        free(p);
    }
    if (!f) {
        record_error(0, "repertoire map file `%s' not found", name);
        return NULL;
    }
    struct repertoire *rep = xcalloc(1, sizeof *rep);
    ht_init(&rep->char_table, 256);
    ht_init(&rep->reverse_table, 256);
    ht_init(&rep->seq_table, 256);
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) >= 0) {
        char *p = line;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '<')
            continue;
        char *e = strchr(p, '>');
        if (!e)
            continue;
        char *q = e + 1;
        while (isspace((unsigned char)*q))
            q++;
        if (q[0] != '<' || q[1] != 'U')
            continue;
        uint32_t v = (uint32_t)strtoul(q + 2, NULL, 16);
        char *nm = xstrndup(p + 1, (size_t)(e - p - 1));
        uint32_t *key = xmalloc(sizeof *key);
        *key = v;
        ht_insert(&rep->char_table, nm, strlen(nm), (void *)(uintptr_t)v);
        ht_insert(&rep->reverse_table, key, sizeof *key, nm);
    }
    free(line);
    fclose(f);
    free(cache_name);
    cache_name = xstrdup(name);
    cache = rep;
    return rep;
}
