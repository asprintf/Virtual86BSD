/*
 * xgettext.c - xgettext: extract gettext call strings from C programs
 * (POSIX.1-2024, XCU xgettext)
 *
 * Synopsis: xgettext [-j] [-n] [-d default-domain] [-K keyword-spec]...
 *                    [-p pathname] file...
 *           xgettext -a [-n] [-d default-domain] [-p pathname]
 *                    [-x exclude-file] file...
 *
 * The C source is read as tokens (comments, character constants and
 * #include lines skipped; lines joined by backslash-newline; adjacent
 * string literals joined, their escapes processed).  A call of a
 * keyword is an identifier followed by "(": its arguments are split at
 * the commas outside parentheses, brackets and braces, and an argument
 * that is only string literals (not in parentheses) is a msgid or a
 * msgid_plural, as the keyword says (when the keyword has both, both
 * must be such, as GNU xgettext has it); keyword calls inside the
 * arguments of others are found too.  The keywords, unless -K "" is given:
 *     gettext gettext_l (1)  ngettext ngettext_l (1,2)
 *     dgettext dgettext_l dcgettext dcgettext_l (2)
 *     dngettext dngettext_l dcngettext dcngettext_l (2,3)
 * and those of -K: ID (1), ID:N, ID:N1,N2.
 *
 * Output: msgid, msgid_plural when there is one, and an empty msgstr (or
 * msgstr[0] and msgstr[1]); with -n, "#: file:line" before.  The strings
 * are written with escapes (cesc.c), a string with newlines inside on
 * lines of its own, each after a newline.  The order is that of the
 * msgids in the files; a msgid met again is written again as comment
 * lines ("# msgid ...").
 *
 * Choices where the standard leaves room:
 *  - The d- and dc- functions (the default keywords) with a string
 *    literal for their domain write to DOMAIN.po (starting with the
 *    directive domain "DOMAIN"); else, and for all other keywords, the
 *    messages go to the default file (messages.po, or that of -d), which
 *    has no domain directive and no header.  With -a there is only the
 *    default file.
 *  - -a: every string literal (joined with those next to it) is a msgid,
 *    in the order of the file, without plurals: the keywords are not
 *    looked for (as GNU xgettext has it).
 *  - The empty string is never a msgid (it is the header's): a warning.
 *  - Strings with prefixes (L"", u8"", u"", U"") are not taken.
 *  - -j: a file that exists is kept as it is (but its domain directives)
 *    and the new messages are appended; those already in it, or met
 *    again, are written as comments.  A file is written only when there
 *    are messages for it, or with -j.
 *  - -x: the msgids of the dot-po file are not extracted.
 *  - "-" is the standard input, named "-" in the "#:" comments.
 *  - Usage errors exit with status 2, other errors with status 1.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "cesc.h"
#include "po.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int status;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("xgettext: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fputs("xgettext: out of memory\n", stderr);
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

/* ---- keywords ---- */

struct keyword {
    char *name;
    int id, plural;             /* argument numbers; plural 0: none */
    int domain;                 /* the domain argument (default keywords), or 0 */
};
static struct keyword *kw;
static int nkw;

static void add_kw(const char *name, size_t len, int id, int plural, int domain)
{
    for (int i = 0; i < nkw; i++)
        if (strlen(kw[i].name) == len && memcmp(kw[i].name, name, len) == 0) {
            kw[i].id = id, kw[i].plural = plural, kw[i].domain = domain;
            return;
        }
    kw = xrealloc(kw, (size_t)(nkw + 1) * sizeof *kw);
    kw[nkw++] = (struct keyword){ xmemdup(name, len), id, plural, domain };
}

static const struct keyword *find_kw(const char *s)
{
    for (int i = 0; i < nkw; i++)
        if (strcmp(kw[i].name, s) == 0)
            return &kw[i];
    return NULL;
}

/* an argument number: decimal, octal 0..., or hexadecimal 0x... */
static bool argnum(const char *s, size_t n, int *v)
{
    char buf[32];
    if (n == 0 || n >= sizeof buf)
        return false;
    memcpy(buf, s, n);
    buf[n] = '\0';
    char *e;
    errno = 0;
    long l = strtol(buf, &e, 0);
    if (*e || errno || l < 1 || l > 1000 || !isdigit((unsigned char)buf[0]))
        return false;
    *v = (int)l;
    return true;
}

static bool keyword_spec(const char *s)
{
    const char *colon = strchr(s, ':');
    size_t nl = colon ? (size_t)(colon - s) : strlen(s);
    if (nl == 0 || !(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return false;
    for (size_t i = 0; i < nl; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_')
            return false;
    int a = 1, b = 0;
    if (colon) {
        const char *comma = strchr(colon + 1, ',');
        if (!argnum(colon + 1, comma ? (size_t)(comma - colon - 1) : strlen(colon + 1), &a))
            return false;
        if (comma && (!argnum(comma + 1, strlen(comma + 1), &b) || b == a))
            return false;
    }
    add_kw(s, nl, a, b, 0);
    return true;
}

/* ---- the messages ---- */

struct msg {
    char *id, *plural;          /* plural NULL if none */
    size_t idlen, plen;
    char *domain;               /* the output file's domain; NULL: default */
    char *file;
    long line;
    long seq;                   /* the position of its msgid */
};
static struct msg *msgs;
static size_t nmsgs, capmsgs;

static char **excluded;
static size_t nexcluded;

static bool is_excluded(const char *id, size_t len)
{
    for (size_t i = 0; i < nexcluded; i++)
        if (strlen(excluded[i]) == len && memcmp(excluded[i], id, len) == 0)
            return true;
    return false;
}

static bool o_a, o_j, o_n;
static const char *o_d = "messages", *o_p;

static void add_msg(const char *id, size_t idlen, const char *pl, size_t plen, const char *domain, const char *file,
                    long line, long seq)
{
    if (idlen == 0) {
        fprintf(stderr, "xgettext: %s:%ld: warning: the empty string is not a msgid\n", file, line);
        return;
    }
    if (is_excluded(id, idlen))
        return;
    if (domain && strcmp(domain, o_d) == 0)
        domain = NULL;          /* the default file's own domain */
    if (nmsgs == capmsgs) {
        capmsgs = capmsgs ? capmsgs * 2 : 64;
        msgs = xrealloc(msgs, capmsgs * sizeof *msgs);
    }
    msgs[nmsgs++] = (struct msg){ xmemdup(id, idlen), pl ? xmemdup(pl, plen) : NULL, idlen, plen,
                                  domain && !o_a ? xmemdup(domain, strlen(domain)) : NULL, (char *)file, line, seq };
}

/* ---- the C tokens ---- */

enum tk { T_END, T_IDENT, T_STRING, T_PUNCT, T_OTHER };

struct src {
    const char *name;
    char *buf;
    size_t len, pos;
    long line;
    long seq;
    long spliced;               /* newlines removed in the current line */
};

/* the next character with backslash-newline removed, or EOF */
static int peekc(struct src *s, size_t *at)
{
    size_t p = *at;
    for (;;) {
        if (p >= s->len)
            return EOF;
        if (s->buf[p] == '\\' && p + 1 < s->len && s->buf[p + 1] == '\n') {
            p += 2;
            continue;
        }
        if (s->buf[p] == '\\' && p + 2 < s->len && s->buf[p + 1] == '\r' && s->buf[p + 2] == '\n') {
            p += 3;
            continue;
        }
        *at = p;
        return (unsigned char)s->buf[p];
    }
}

static int getc_src(struct src *s)
{
    size_t p = s->pos;
    /* the spliced newlines count when the line they are in ends (so a
       token is on the line where its logical line starts, as with GNU
       xgettext) */
    for (size_t q = s->pos; q < s->len; ) {
        if (s->buf[q] == '\\' && q + 1 < s->len && s->buf[q + 1] == '\n') {
            s->spliced++;
            q += 2;
        } else if (s->buf[q] == '\\' && q + 2 < s->len && s->buf[q + 1] == '\r' && s->buf[q + 2] == '\n') {
            s->spliced++;
            q += 3;
        } else {
            break;
        }
    }
    int c = peekc(s, &p);
    if (c == EOF) {
        s->pos = s->len;
        return EOF;
    }
    s->pos = p + 1;
    if (c == '\n') {
        s->line += 1 + s->spliced;
        s->spliced = 0;
    }
    return c;
}

static int look(struct src *s)
{
    size_t p = s->pos;
    return peekc(s, &p);
}

static int look2(struct src *s)
{
    size_t p = s->pos;
    if (peekc(s, &p) == EOF)
        return EOF;
    p++;
    return peekc(s, &p);
}

struct token {
    enum tk type;
    char *text;                 /* identifier, or the string's bytes */
    size_t len;
    char punct;
    long line, seq;
};

static char *tbuf;
static size_t tcap;

static void tput(size_t *n, int c)
{
    if (*n + 1 >= tcap) {
        tcap = tcap ? tcap * 2 : 256;
        tbuf = xrealloc(tbuf, tcap);
    }
    tbuf[(*n)++] = (char)c;
}

static bool at_line_start;

/* read a quoted literal whose opening quote q was read; the raw inside */
static size_t literal(struct src *s, int q)
{
    size_t n = 0;
    int c;
    while ((c = getc_src(s)) != EOF && c != q && c != '\n') {
        tput(&n, c);
        if (c == '\\') {
            c = getc_src(s);
            if (c == EOF)
                break;
            tput(&n, c);
        }
    }
    if (c != q)
        fprintf(stderr, "xgettext: %s:%ld: warning: a literal without its closing quote\n", s->name, s->line);
    return n;
}

static void skip_line(struct src *s)
{
    int c;
    while ((c = look(s)) != EOF && c != '\n') {
        if (c == '/' && look2(s) == '*')
            break;              /* a comment may go on past the line */
        getc_src(s);
    }
}

static bool next_token(struct src *s, struct token *t)
{
    for (;;) {
        int c = getc_src(s);
        if (c == EOF) {
            t->type = T_END;
            return false;
        }
        if (c == '\n') {
            at_line_start = true;
            continue;
        }
        if (isspace(c))
            continue;
        if (c == '/' && look(s) == '*') {
            getc_src(s);
            int p = 0;
            while ((c = getc_src(s)) != EOF && !(p == '*' && c == '/'))
                p = c;
            continue;
        }
        if (c == '/' && look(s) == '/') {
            while ((c = look(s)) != EOF && c != '\n')
                getc_src(s);
            continue;
        }
        bool start = at_line_start;
        at_line_start = false;
        t->line = s->line;
        t->seq = s->seq++;
        if (c == '#' && start) {
            /* a directive: #include lines are skipped */
            size_t n = 0;
            while (look(s) == ' ' || look(s) == '\t')
                getc_src(s);
            while (isalpha(look(s)))
                tput(&n, getc_src(s));
            tput(&n, 0);
            if (strcmp(tbuf, "include") == 0 || strcmp(tbuf, "include_next") == 0 || strcmp(tbuf, "line") == 0
                || strcmp(tbuf, "pragma") == 0 || strcmp(tbuf, "error") == 0 || strcmp(tbuf, "warning") == 0) {
                skip_line(s);
                continue;
            }
            t->type = T_OTHER;
            return true;
        }
        if (c == '"') {
            size_t n = literal(s, '"');
            t->type = T_STRING;
            t->text = tbuf;
            t->len = n;
            return true;
        }
        if (c == '\'') {
            literal(s, '\'');
            t->type = T_OTHER;
            return true;
        }
        if (isalpha(c) || c == '_') {
            size_t n = 0;
            tput(&n, c);
            while (isalnum(look(s)) || look(s) == '_')
                tput(&n, getc_src(s));
            tput(&n, 0);
            /* prefixed literals */
            if ((look(s) == '"' || look(s) == '\'')
                && (strcmp(tbuf, "L") == 0 || strcmp(tbuf, "u") == 0 || strcmp(tbuf, "U") == 0
                    || strcmp(tbuf, "u8") == 0)) {
                literal(s, getc_src(s));
                t->type = T_OTHER;
                return true;
            }
            t->type = T_IDENT;
            t->text = tbuf;
            t->len = n - 1;
            return true;
        }
        if (isdigit(c) || (c == '.' && isdigit(look(s)))) {
            while (isalnum(look(s)) || look(s) == '.' || look(s) == '_'
                   || ((look(s) == '+' || look(s) == '-') && strchr("eEpP", c)))
                c = getc_src(s);
            t->type = T_OTHER;
            return true;
        }
        t->type = T_PUNCT;
        t->punct = (char)c;
        return true;
    }
}

/* ---- the extraction ---- */

struct arg {
    char *s;                    /* the joined strings, escapes processed */
    size_t len;
    bool strings_only, any;
    long line, seq;
};

struct frame {
    const struct keyword *k;
    int depth;                  /* of parentheses and the like, inside it */
    int argno;                  /* 1-based */
    struct arg args[3];         /* the ones the keyword wants: id, plural, domain */
    int want[3];
    long line;
};

static void arg_reset(struct arg *a)
{
    free(a->s);
    memset(a, 0, sizeof *a);
    a->strings_only = true;
}

static int slot(const struct frame *f)
{
    for (int i = 0; i < 3; i++)
        if (f->want[i] == f->argno)
            return i;
    return -1;
}

/* the string literal token t processed */
static char *process(const struct token *t, size_t *len, const char *file)
{
    char *out = xmalloc(t->len + 1);
    if (cesc(t->text, t->len, out, len, CESC_STRICT) != 0)
        fprintf(stderr, "xgettext: %s:%ld: warning: an invalid escape sequence\n", file, t->line);
    return out;
}

static void extract(struct src *s)
{
    struct frame *stack = NULL;
    int nst = 0, capst = 0;
    struct token t, prev = { T_END, NULL, 0, 0, 0, 0 };
    char *prev_ident = NULL;
    /* -a: the string literals outside keyword arguments, joined */
    char *loose = NULL;
    size_t looselen = 0;
    long loose_line = 0, loose_seq = 0;
    bool loose_open = false, prev_string = false;

    while (next_token(s, &t)) {
        bool is_string = t.type == T_STRING;
        /* -a: the end of a run of loose strings */
        if (o_a && loose_open && !is_string) {
            add_msg(loose, looselen, NULL, 0, NULL, s->name, loose_line, loose_seq);
            free(loose);
            loose = NULL;
            loose_open = false;
        }
        struct frame *f = nst ? &stack[nst - 1] : NULL;
        if (!o_a && t.type == T_PUNCT && t.punct == '(' && prev.type == T_IDENT && prev_ident && find_kw(prev_ident)) {
            if (f) {
                int k = slot(f);
                if (k >= 0)
                    f->args[k].strings_only = false;
            }
            if (nst == capst) {
                capst = capst ? capst * 2 : 8;
                stack = xrealloc(stack, (size_t)capst * sizeof *stack);
            }
            f = &stack[nst++];
            memset(f, 0, sizeof *f);
            f->k = find_kw(prev_ident);
            f->argno = 1;
            f->want[0] = f->k->id;
            f->want[1] = f->k->plural;
            f->want[2] = f->k->domain;
            f->line = prev.line;
            for (int i = 0; i < 3; i++)
                arg_reset(&f->args[i]);
        } else if (f) {
            int k = slot(f);
            if (t.type == T_PUNCT && strchr("([{", t.punct)) {
                f->depth++;
            } else if (t.type == T_PUNCT && strchr(")]}", t.punct)) {
                if (f->depth > 0) {
                    f->depth--;
                } else if (t.punct == ')') {
                    /* the end of the call */
                    struct arg *id = &f->args[0], *pl = &f->args[1], *dm = &f->args[2];
                    bool pl_ok = f->want[1] && pl->any && pl->strings_only;
                    bool id_ok = id->any && id->strings_only && (!f->want[1] || pl_ok);
                    bool dm_ok = f->want[2] && dm->any && dm->strings_only;
                    if (id_ok)
                        add_msg(id->s, id->len, pl_ok ? pl->s : NULL, pl->len,
                                dm_ok ? dm->s : NULL, s->name, id->line, id->seq);
                    for (int i = 0; i < 3; i++)
                        arg_reset(&f->args[i]);
                    nst--;
                    f = nst ? &stack[nst - 1] : NULL;
                    if (f) {
                        int k2 = slot(f);
                        if (k2 >= 0)
                            f->args[k2].strings_only = false;
                    }
                    prev = t;
                    prev_string = false;
                    continue;
                }
                if (k >= 0 && t.punct != ')')
                    f->args[k].strings_only = false;
            } else if (t.type == T_PUNCT && t.punct == ',' && f->depth == 0) {
                f->argno++;
                prev = t;
                prev_string = false;
                continue;
            }
            if (k >= 0) {
                struct arg *a = &f->args[k];
                if (is_string && f->depth == 0) {
                    size_t len;
                    char *v = process(&t, &len, s->name);
                    if (!a->any) {
                        a->line = t.line;
                        a->seq = t.seq;
                    }
                    a->s = xrealloc(a->s, a->len + len + 1);
                    memcpy(a->s + a->len, v, len);
                    a->len += len;
                    a->s[a->len] = '\0';
                    free(v);
                    a->any = true;
                } else {
                    a->strings_only = false;
                }
            }
        } else if (o_a && is_string) {
            goto loose_string;
        }
        if (t.type == T_IDENT) {
            free(prev_ident);
            prev_ident = xmemdup(t.text, t.len);
        }
        prev = t;
        prev_string = is_string;
        continue;
    loose_string: {
            size_t len;
            char *v = process(&t, &len, s->name);
            if (!loose_open) {
                loose_open = true;
                loose_line = t.line;
                loose_seq = t.seq;
                looselen = 0;
            }
            loose = xrealloc(loose, looselen + len + 1);
            memcpy(loose + looselen, v, len);
            looselen += len;
            free(v);
            prev = t;
            prev_string = true;
        }
    }
    if (o_a && loose_open) {
        add_msg(loose, looselen, NULL, 0, NULL, s->name, loose_line, loose_seq);
        free(loose);
    }
    (void)prev_string;
    for (int i = 0; i < nst; i++)
        for (int k = 0; k < 3; k++)
            arg_reset(&stack[i].args[k]);
    free(stack);
    free(prev_ident);
}

static bool read_source(const char *name)
{
    FILE *f = strcmp(name, "-") == 0 ? stdin : fopen(name, "r");
    if (!f) {
        fprintf(stderr, "xgettext: %s: %s\n", name, strerror(errno));
        return false;
    }
    struct src s = { name, NULL, 0, 0, 1, 0, 0 };
    size_t cap = 0;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (s.len + n > cap) {
            cap = (s.len + n) * 2;
            s.buf = xrealloc(s.buf, cap);
        }
        memcpy(s.buf + s.len, buf, n);
        s.len += n;
    }
    bool ok = !ferror(f);
    if (!ok)
        fprintf(stderr, "xgettext: %s: %s\n", name, strerror(errno));
    if (f != stdin)
        fclose(f);
    at_line_start = true;
    extract(&s);
    free(s.buf);
    return ok;
}

/* ---- the output ---- */

struct seen {
    char **ids;
    size_t *lens;
    size_t n, cap;
};

static bool seen_has(const struct seen *s, const char *id, size_t len)
{
    for (size_t i = 0; i < s->n; i++)
        if (s->lens[i] == len && memcmp(s->ids[i], id, len) == 0)
            return true;
    return false;
}

static void seen_add(struct seen *s, const char *id, size_t len)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->ids = xrealloc(s->ids, s->cap * sizeof *s->ids);
        s->lens = xrealloc(s->lens, s->cap * sizeof *s->lens);
    }
    s->ids[s->n] = xmemdup(id, len);
    s->lens[s->n++] = len;
}

static void put_string(FILE *o, const char *prefix, const char *directive, const char *s, size_t len)
{
    char *q = xmalloc(4 * len + 1);
    /* a newline inside: each line after it on a line of its own */
    const char *nl = memchr(s, '\n', len);
    if (!nl || nl == s + len - 1) {
        cesc_quote(s, len, q);
        fprintf(o, "%s%s \"%s\"\n", prefix, directive, q);
    } else {
        fprintf(o, "%s%s \"\"\n", prefix, directive);
        size_t i = 0;
        while (i < len) {
            const char *e = memchr(s + i, '\n', len - i);
            size_t l = e ? (size_t)(e - (s + i)) + 1 : len - i;
            cesc_quote(s + i, l, q);
            fprintf(o, "%s\"%s\"\n", prefix, q);
            i += l;
        }
    }
    free(q);
}

static void put_msg(FILE *o, const struct msg *m, bool comment)
{
    const char *pre = comment ? "# " : "";
    if (o_n)
        fprintf(o, "#: %s:%ld\n", m->file, m->line);
    put_string(o, pre, "msgid", m->id, m->idlen);
    if (m->plural) {
        put_string(o, pre, "msgid_plural", m->plural, m->plen);
        fprintf(o, "%smsgstr[0] \"\"\n%smsgstr[1] \"\"\n", pre, pre);
    } else {
        fprintf(o, "%smsgstr \"\"\n", pre);
    }
}

/* the existing file (for -j): its text without domain directives, and its
   msgids in seen */
static void take_existing(const struct po_msg *m, const char *domain, void *arg)
{
    (void)domain;
    if (m && m->id.len > 0)
        seen_add(arg, m->id.s, m->id.len);
}

static bool write_po(const char *domain, bool is_default)
{
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s%s%s.po", o_p ? o_p : "", o_p ? "/" : "", domain) >= (int)sizeof path) {
        fprintf(stderr, "xgettext: %s: name too long\n", domain);
        return false;
    }
    struct seen seen = { 0 };
    char *old = NULL;
    size_t oldlen = 0;
    if (o_j) {
        FILE *f = fopen(path, "r");
        if (f) {
            int errs = po_read(f, path, "xgettext", take_existing, &seen);
            rewind(f);
            char *line = NULL;
            size_t cap = 0;
            ssize_t n;
            while ((n = getline(&line, &cap, f)) >= 0) {
                char *s = line;
                while (*s == ' ' || *s == '\t')
                    s++;
                if (strncmp(s, "domain", 6) == 0 && (s[6] == ' ' || s[6] == '\t'))
                    continue;
                old = xrealloc(old, oldlen + (size_t)n + 1);
                memcpy(old + oldlen, line, (size_t)n);
                oldlen += (size_t)n;
            }
            free(line);
            fclose(f);
            if (errs) {
                fprintf(stderr, "xgettext: %s: not joined\n", path);
                free(old);
                return false;
            }
        } else if (errno != ENOENT) {
            fprintf(stderr, "xgettext: %s: %s\n", path, strerror(errno));
            return false;
        }
    }
    FILE *o = fopen(path, "w");
    if (!o) {
        fprintf(stderr, "xgettext: %s: %s\n", path, strerror(errno));
        free(old);
        return false;
    }
    bool first = true;
    if (!is_default) {
        fprintf(o, "domain \"%s\"\n", domain);
        first = false;
    }
    if (oldlen) {
        if (!first)
            fputc('\n', o);
        fwrite(old, 1, oldlen, o);
        first = false;
        if (old[oldlen - 1] != '\n')
            fputc('\n', o);
    }
    free(old);
    for (size_t i = 0; i < nmsgs; i++) {
        const struct msg *m = &msgs[i];
        if (is_default ? m->domain != NULL : !m->domain || strcmp(m->domain, domain) != 0)
            continue;
        if (!first)
            fputc('\n', o);
        first = false;
        bool dup = seen_has(&seen, m->id, m->idlen);
        put_msg(o, m, dup);
        if (!dup)
            seen_add(&seen, m->id, m->idlen);
    }
    for (size_t i = 0; i < seen.n; i++)
        free(seen.ids[i]);
    free(seen.ids);
    free(seen.lens);
    if (fclose(o) != 0) {
        fprintf(stderr, "xgettext: %s: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

static void take_excluded(const struct po_msg *m, const char *domain, void *arg)
{
    (void)domain;
    (void)arg;
    if (!m)
        return;
    excluded = xrealloc(excluded, (nexcluded + 1) * sizeof *excluded);
    excluded[nexcluded++] = xmemdup(m->id.s, m->id.len);
}

static void usage(void)
{
    fputs("usage: xgettext [-j] [-n] [-d default-domain] [-K keyword-spec]... [-p pathname] file...\n"
          "       xgettext -a [-n] [-d default-domain] [-p pathname] [-x exclude-file] file...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bool defaults = true;
    char **specs = xmalloc((size_t)argc * sizeof *specs);
    int nspecs = 0;
    const char *o_x = NULL;
    int c;
    while ((c = getopt(argc, argv, "ajnd:K:p:x:")) != -1) {
        switch (c) {
        case 'a': o_a = true; break;
        case 'j': o_j = true; break;
        case 'n': o_n = true; break;
        case 'd': o_d = optarg; break;
        case 'K':
            if (!*optarg)
                defaults = false;
            else
                specs[nspecs++] = optarg;
            break;
        case 'p': o_p = optarg; break;
        case 'x': o_x = optarg; break;
        default: usage();
        }
    }
    if (optind >= argc)
        usage();
    if (!*o_d || strchr(o_d, '/')) {
        fprintf(stderr, "xgettext: invalid domain: %s\n", o_d);
        return 2;
    }
    if (defaults) {
        static const struct { const char *n; int id, pl, dm; } d[] = {
            { "gettext", 1, 0, 0 }, { "gettext_l", 1, 0, 0 }, { "ngettext", 1, 2, 0 }, { "ngettext_l", 1, 2, 0 },
            { "dgettext", 2, 0, 1 }, { "dgettext_l", 2, 0, 1 }, { "dcgettext", 2, 0, 1 },
            { "dcgettext_l", 2, 0, 1 }, { "dngettext", 2, 3, 1 }, { "dngettext_l", 2, 3, 1 },
            { "dcngettext", 2, 3, 1 }, { "dcngettext_l", 2, 3, 1 },
        };
        for (size_t i = 0; i < sizeof d / sizeof d[0]; i++)
            add_kw(d[i].n, strlen(d[i].n), d[i].id, d[i].pl, d[i].dm);
    }
    for (int i = 0; i < nspecs; i++)
        if (!keyword_spec(specs[i])) {
            fprintf(stderr, "xgettext: invalid keyword specification: %s\n", specs[i]);
            return 2;
        }
    if (o_x) {
        FILE *f = fopen(o_x, "r");
        if (!f) {
            fprintf(stderr, "xgettext: %s: %s\n", o_x, strerror(errno));
            return 1;
        }
        int errs = po_read(f, o_x, "xgettext", take_excluded, NULL);
        fclose(f);
        if (errs)
            return 1;
    }
    for (int i = optind; i < argc; i++)
        if (!read_source(argv[i]))
            status = 1;

    /* the files: the default one, then each domain's */
    bool any_default = o_j;
    for (size_t i = 0; i < nmsgs; i++)
        if (!msgs[i].domain)
            any_default = true;
    if (any_default && !write_po(o_d, true))
        status = 1;
    for (size_t i = 0; i < nmsgs; i++) {
        const char *d = msgs[i].domain;
        if (!d)
            continue;
        bool done = false;
        for (size_t k = 0; k < i && !done; k++)
            if (msgs[k].domain && strcmp(msgs[k].domain, d) == 0)
                done = true;
        if (!done && !write_po(d, false))
            status = 1;
    }
    return status;
}
