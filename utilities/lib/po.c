/*
 * po.c - dot-po files (see po.h)
 *
 * The format of the standard (XCU msgfmt): comments (# to the end of the
 * line; "#," comments hold flags: fuzzy, c-format, no-c-format), blank
 * lines, and statements "directive value" with a value of one or more
 * double-quoted strings, the others each on a line of its own, which are
 * joined.  The directives: domain, msgid, msgid_plural, msgstr and
 * msgstr[index]; and msgctxt, an extension of GNU gettext.  In the
 * strings, the escapes of C (cesc.c), an unknown escape being an error.
 * Comments starting with "#~" (GNU's obsolete messages) are comments.
 */

#define _POSIX_C_SOURCE 202405L

#include "po.h"
#include "cesc.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct reader {
    FILE *f;
    const char *name, *prog;
    long lineno;
    char *line;
    size_t cap;
    bool pushed;                /* the line is to be read again */
    bool eof;
    int errors;
};

static void error(struct reader *r, long line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: %s:%ld: ", r->prog, r->name, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    r->errors++;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fputs("out of memory\n", stderr);
        exit(1);
    }
    return p;
}

/* the next line, without its newline; NULL at the end */
static char *next_line(struct reader *r)
{
    if (r->pushed) {
        r->pushed = false;
        return r->line;
    }
    if (r->eof)
        return NULL;
    ssize_t n = getline(&r->line, &r->cap, r->f);
    if (n < 0) {
        r->eof = true;
        return NULL;
    }
    r->lineno++;
    if (n > 0 && r->line[n - 1] == '\n')
        r->line[--n] = '\0';
    if (n > 0 && r->line[n - 1] == '\r')
        r->line[--n] = '\0';
    return r->line;
}

static char *skip_blanks(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* a quoted string at *p, appended to str; false if it is not one */
static bool quoted(struct reader *r, char **p, struct po_str *str)
{
    char *s = *p;
    if (*s != '"')
        return false;
    s++;
    char *start = s;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1])
            s++;
        s++;
    }
    if (*s != '"') {
        error(r, r->lineno, "a string without its closing quote");
        return false;
    }
    size_t n = (size_t)(s - start), len;
    str->s = xrealloc(str->s, str->len + n + 1);
    if (cesc(start, n, str->s + str->len, &len, CESC_STRICT) != 0)
        error(r, r->lineno, "an invalid escape sequence");
    str->len += len;
    str->s[str->len] = '\0';
    *p = skip_blanks(s + 1);
    if (**p) {
        error(r, r->lineno, "something after a string");
        return false;
    }
    return true;
}

/* the value of a statement at p and its continuation lines */
static bool value(struct reader *r, char *p, struct po_str *str)
{
    str->s = xrealloc(NULL, 1);
    str->s[0] = '\0';
    str->len = 0;
    p = skip_blanks(p);
    if (*p != '"') {
        error(r, r->lineno, "a directive without a string");
        return false;
    }
    if (!quoted(r, &p, str))
        return false;
    char *l;
    while ((l = next_line(r))) {
        char *s = skip_blanks(l);
        if (*s != '"') {
            r->pushed = true;
            break;
        }
        if (!quoted(r, &s, str))
            return false;
    }
    return true;
}

/* the directive of line l: its name in d (of size n), the rest after it */
static char *directive(char *l, char *d, size_t n)
{
    char *s = skip_blanks(l);
    size_t k = 0;
    while (s[k] && s[k] != ' ' && s[k] != '\t' && s[k] != '"' && k + 1 < n)
        k++;
    memcpy(d, s, k);
    d[k] = '\0';
    return s + k;
}

static void free_msg(struct po_msg *m)
{
    free(m->ctxt.s);
    free(m->id.s);
    free(m->plural.s);
    for (int i = 0; i < m->nstrs; i++)
        free(m->strs[i].s);
    free(m->strs);
}

int po_read(FILE *f, const char *name, const char *prog, po_fn *fn, void *arg)
{
    struct reader r = { f, name, prog, 0, NULL, 0, false, false, 0 };
    char *domain = NULL;
    bool fuzzy = false;
    int cformat = 0;
    char *l;
    char d[64];
    while ((l = next_line(&r))) {
        char *s = skip_blanks(l);
        if (!*s)
            continue;
        if (*s == '#') {
            if (s[1] == ',') {
                char *copy = strdup(s + 2), *save = NULL;
                if (!copy)
                    exit(1);
                for (char *t = strtok_r(copy, ", \t", &save); t; t = strtok_r(NULL, ", \t", &save)) {
                    if (strcmp(t, "fuzzy") == 0)
                        fuzzy = true;
                    else if (strcmp(t, "c-format") == 0)
                        cformat = 1;
                    else if (strcmp(t, "no-c-format") == 0)
                        cformat = -1;
                }
                free(copy);
            }
            continue;
        }
        char *rest = directive(l, d, sizeof d);
        long line = r.lineno;
        if (strcmp(d, "domain") == 0) {
            struct po_str v = { 0 };
            if (value(&r, rest, &v)) {
                if (v.len == 0 || memchr(v.s, '\0', v.len) || strchr(v.s, '/'))
                    error(&r, line, "an invalid domain name");
                else {
                    free(domain);
                    domain = v.s;
                    v.s = NULL;
                    fn(NULL, domain, arg);
                }
            }
            free(v.s);
            fuzzy = false;
            cformat = 0;
            continue;
        }
        struct po_msg m;
        memset(&m, 0, sizeof m);
        m.domain = domain ? domain : "messages";
        m.file = name;
        if (strcmp(d, "msgctxt") == 0) {
            if (!value(&r, rest, &m.ctxt)) {
                free_msg(&m);
                continue;
            }
            l = next_line(&r);
            if (!l) {
                error(&r, line, "msgctxt without msgid");
                free_msg(&m);
                break;
            }
            rest = directive(l, d, sizeof d);
            line = r.lineno;
        }
        if (strcmp(d, "msgid") != 0) {
            if (d[0])
                error(&r, line, "%s where msgid was expected", d);
            else
                error(&r, line, "a string out of place");
            free_msg(&m);
            fuzzy = false;
            cformat = 0;
            continue;
        }
        m.line = line;
        m.fuzzy = fuzzy;
        m.cformat = cformat;
        fuzzy = false;
        cformat = 0;
        if (!value(&r, rest, &m.id)) {
            free_msg(&m);
            continue;
        }
        /* msgid_plural, then msgstr[0..]; or msgstr */
        l = next_line(&r);
        while (l && (*skip_blanks(l) == '\0' || *skip_blanks(l) == '#'))
            l = next_line(&r);      /* comments between are not expected, but harmless */
        if (!l) {
            error(&r, line, "msgid without msgstr");
            free_msg(&m);
            break;
        }
        rest = directive(l, d, sizeof d);
        if (strcmp(d, "msgid_plural") == 0) {
            if (!value(&r, rest, &m.plural)) {
                free_msg(&m);
                continue;
            }
            m.indexed = true;
            for (;;) {
                l = next_line(&r);
                if (!l)
                    break;
                rest = directive(l, d, sizeof d);
                char *e;
                long idx;
                if (strncmp(d, "msgstr[", 7) != 0 || !isdigit((unsigned char)d[7])
                    || (idx = strtol(d + 7, &e, 10), strcmp(e, "]") != 0)) {
                    r.pushed = true;
                    break;
                }
                if (idx != m.nstrs)
                    error(&r, r.lineno, "msgstr[%ld] where msgstr[%d] was expected", idx, m.nstrs);
                m.strs = xrealloc(m.strs, (size_t)(m.nstrs + 1) * sizeof *m.strs);
                memset(&m.strs[m.nstrs], 0, sizeof *m.strs);
                bool ok = value(&r, rest, &m.strs[m.nstrs]);
                m.nstrs++;
                if (!ok)
                    break;
            }
            if (m.nstrs == 0)
                error(&r, line, "msgid_plural without msgstr[0]");
        } else if (strcmp(d, "msgstr") == 0) {
            m.strs = xrealloc(NULL, sizeof *m.strs);
            memset(m.strs, 0, sizeof *m.strs);
            m.nstrs = 1;
            if (!value(&r, rest, &m.strs[0])) {
                free_msg(&m);
                continue;
            }
        } else {
            error(&r, r.lineno, "msgid without msgstr");
            r.pushed = true;
            free_msg(&m);
            continue;
        }
        if (m.nstrs > 0)
            fn(&m, m.domain, arg);
        free_msg(&m);
    }
    if (ferror(f)) {
        fprintf(stderr, "%s: %s: read error\n", prog, name);
        r.errors++;
    }
    free(r.line);
    free(domain);
    return r.errors;
}
