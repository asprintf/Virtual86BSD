/*
 * tl.c - tl, the test language of the tests of these utilities (and a
 * program of the system, /usr/bin/tl): a small language in the style of
 * C, interpreted, with the interfaces of POSIX as its library and a
 * pseudo-terminal with a screen (a terminal emulator) for the tests of
 * interactive utilities.  See tl.md.
 *
 * Usage: tl script [argument...]
 *        tl -e program [argument...]
 *
 * The values: nil, integers (64 bits), doubles, strings (of bytes, any
 * bytes), lists, maps (keys: strings and integers, in the order they were
 * put), functions and terminals.  Strings are values; lists and maps are
 * shared by reference.  Memory is reference counted.
 *
 * Exit status: what exit() is given, 0 at the end of the script, 2 after
 * an error of the program (a message with its file and line).
 */

#define _XOPEN_SOURCE 800

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wctype.h>
#include <dirent.h>

#include "../../lib/libxcu/port.h"

#if defined(__linux__)
/* not in POSIX (an extension of the C libraries of Linux) */
extern int setgroups(size_t, const gid_t *);
#endif

extern char **environ;

/* ---- values ---- */

enum { T_NIL, T_INT, T_FLT, T_STR, T_LIST, T_MAP, T_FUNC, T_BUILTIN, T_TERM };

typedef struct Str Str;
typedef struct List List;
typedef struct Map Map;
typedef struct Func Func;
typedef struct Builtin Builtin;
typedef struct Term Term;
typedef struct Node Node;

typedef struct Value {
    int t;
    union {
        int64_t i;
        double f;
        Str *s;
        List *l;
        Map *m;
        Func *fn;
        const Builtin *b;
        Term *term;
    } u;
} Value;

struct Str {
    long rc;
    size_t len, cap;
    char *p;                    /* len bytes and a NUL */
};

struct List {
    long rc;
    size_t n, cap;
    Value *v;
};

struct Map {
    long rc;
    size_t n, cap;              /* the entries, in order */
    Value *k, *v;
    size_t hcap;                /* the index: entry + 1, 0 empty */
    size_t *h;
};

static const Value NIL = { T_NIL, { 0 } };

/* floor, fabs and fmod, without the math library */
static double dfloor(double x)
{
    if (x != x || x >= 9.2e18 || x <= -9.2e18)
        return x;
    double t = (double)(int64_t)x;
    return t > x ? t - 1 : t;
}

static double dfabs(double x)
{
    return x < 0 ? -x : x;
}

static double dfmod(double x, double y)
{
    if (y == 0)
        return x - x;           /* NaN for x/0 */
    double q = x / y;
    q = q < 0 ? -dfloor(-q) : dfloor(q);
    return x - q * y;
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("tl: out of memory\n", stderr);
        exit(2);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fputs("tl: out of memory\n", stderr);
        exit(2);
    }
    return p;
}

static void *xcalloc(size_t n, size_t m)
{
    void *p = calloc(n ? n : 1, m ? m : 1);
    if (!p) {
        fputs("tl: out of memory\n", stderr);
        exit(2);
    }
    return p;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static Value vint(int64_t i)
{
    Value v;
    v.t = T_INT;
    v.u.i = i;
    return v;
}

static Value vflt(double f)
{
    Value v;
    v.t = T_FLT;
    v.u.f = f;
    return v;
}

static Str *str_new(const char *p, size_t n)
{
    Str *s = xmalloc(sizeof *s);
    s->rc = 1;
    s->len = n;
    s->cap = n + 1;
    s->p = xmalloc(s->cap);
    if (n)
        memcpy(s->p, p, n);
    s->p[n] = '\0';
    return s;
}

static Value vstrn(const char *p, size_t n)
{
    Value v;
    v.t = T_STR;
    v.u.s = str_new(p, n);
    return v;
}

static Value vstr(const char *p)
{
    return vstrn(p, strlen(p));
}

static Value vstr_take(Str *s)
{
    Value v;
    v.t = T_STR;
    v.u.s = s;
    return v;
}

static void str_append(Str *s, const char *p, size_t n)
{
    if (s->len + n + 1 > s->cap) {
        size_t c = s->cap * 2;
        if (c < s->len + n + 1)
            c = s->len + n + 1;
        s->p = xrealloc(s->p, c);
        s->cap = c;
    }
    if (n)
        memcpy(s->p + s->len, p, n);
    s->len += n;
    s->p[s->len] = '\0';
}

static void str_putc(Str *s, int c)
{
    char ch = (char)c;
    str_append(s, &ch, 1);
}

/* a growing buffer of bytes (a Str not yet a value) */
static Str *buf_new(void)
{
    return str_new("", 0);
}

static void buf_printf(Str *s, const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n < sizeof tmp) {
        str_append(s, tmp, (size_t)n);
        return;
    }
    char *big = xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    str_append(s, big, (size_t)n);
    free(big);
}

static List *list_new(size_t cap)
{
    List *l = xmalloc(sizeof *l);
    l->rc = 1;
    l->n = 0;
    l->cap = cap ? cap : 4;
    l->v = xmalloc(l->cap * sizeof *l->v);
    return l;
}

static Value vlist(List *l)
{
    Value v;
    v.t = T_LIST;
    v.u.l = l;
    return v;
}

static Map *map_new(void)
{
    Map *m = xcalloc(1, sizeof *m);
    m->rc = 1;
    return m;
}

static Value vmap(Map *m)
{
    Value v;
    v.t = T_MAP;
    v.u.m = m;
    return v;
}

static void decref(Value v);

static Value incref(Value v)
{
    switch (v.t) {
    case T_STR: v.u.s->rc++; break;
    case T_LIST: v.u.l->rc++; break;
    case T_MAP: v.u.m->rc++; break;
    default: break;
    }
    return v;
}

static void decref(Value v)
{
    switch (v.t) {
    case T_STR:
        if (--v.u.s->rc == 0) {
            free(v.u.s->p);
            free(v.u.s);
        }
        break;
    case T_LIST:
        if (--v.u.l->rc == 0) {
            List *l = v.u.l;
            for (size_t i = 0; i < l->n; i++)
                decref(l->v[i]);
            free(l->v);
            free(l);
        }
        break;
    case T_MAP:
        if (--v.u.m->rc == 0) {
            Map *m = v.u.m;
            for (size_t i = 0; i < m->n; i++) {
                decref(m->k[i]);
                decref(m->v[i]);
            }
            free(m->k);
            free(m->v);
            free(m->h);
            free(m);
        }
        break;
    default:
        break;
    }
}

static void list_push(List *l, Value v)    /* takes v */
{
    if (l->n == l->cap) {
        l->cap *= 2;
        l->v = xrealloc(l->v, l->cap * sizeof *l->v);
    }
    l->v[l->n++] = v;
}

/* ---- maps ---- */

static uint64_t hash_bytes(const char *p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t key_hash(Value k)
{
    if (k.t == T_STR)
        return hash_bytes(k.u.s->p, k.u.s->len);
    uint64_t x = (uint64_t)k.u.i;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return x;
}

static int key_eq(Value a, Value b)
{
    if (a.t != b.t)
        return 0;
    if (a.t == T_STR)
        return a.u.s->len == b.u.s->len && memcmp(a.u.s->p, b.u.s->p, a.u.s->len) == 0;
    return a.u.i == b.u.i;
}

static void map_reindex(Map *m)
{
    free(m->h);
    m->hcap = 8;
    while (m->hcap < m->n * 2 + 2)
        m->hcap *= 2;
    m->h = xcalloc(m->hcap, sizeof *m->h);
    for (size_t i = 0; i < m->n; i++) {
        size_t j = key_hash(m->k[i]) & (m->hcap - 1);
        while (m->h[j])
            j = (j + 1) & (m->hcap - 1);
        m->h[j] = i + 1;
    }
}

/* the entry of key k, or -1 */
static long map_find(const Map *m, Value k)
{
    if (!m->hcap)
        return -1;
    size_t j = key_hash(k) & (m->hcap - 1);
    while (m->h[j]) {
        size_t e = m->h[j] - 1;
        if (key_eq(m->k[e], k))
            return (long)e;
        j = (j + 1) & (m->hcap - 1);
    }
    return -1;
}

/* takes k and v */
static void map_set(Map *m, Value k, Value v)
{
    long e = map_find(m, k);
    if (e >= 0) {
        decref(k);
        decref(m->v[e]);
        m->v[e] = v;
        return;
    }
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->k = xrealloc(m->k, m->cap * sizeof *m->k);
        m->v = xrealloc(m->v, m->cap * sizeof *m->v);
    }
    m->k[m->n] = k;
    m->v[m->n] = v;
    m->n++;
    if (m->n * 2 + 2 > m->hcap)
        map_reindex(m);
    else {
        size_t j = key_hash(k) & (m->hcap - 1);
        while (m->h[j])
            j = (j + 1) & (m->hcap - 1);
        m->h[j] = m->n;
    }
}

static void map_sets(Map *m, const char *k, Value v)
{
    map_set(m, vstr(k), v);
}

static int map_del(Map *m, Value k)
{
    long e = map_find(m, k);
    if (e < 0)
        return 0;
    decref(m->k[e]);
    decref(m->v[e]);
    memmove(&m->k[e], &m->k[e + 1], (m->n - (size_t)e - 1) * sizeof *m->k);
    memmove(&m->v[e], &m->v[e + 1], (m->n - (size_t)e - 1) * sizeof *m->v);
    m->n--;
    map_reindex(m);
    return 1;
}

/* a borrowed value, or NIL */
static Value map_get(const Map *m, Value k)
{
    long e = map_find(m, k);
    return e < 0 ? NIL : m->v[e];
}

static Value map_gets(const Map *m, const char *k)
{
    Str s = { 1, strlen(k), 0, (char *)k };
    Value kv;
    kv.t = T_STR;
    kv.u.s = &s;
    return map_get(m, kv);
}

/* ---- errors ---- */

struct Func {
    const char *name;
    int nparams, nlocals;
    Node *body;
    const char *file;
    int line;
};

typedef Value (*BuiltinFn)(Value *a, int n);

struct Builtin {
    const char *name;
    BuiltinFn fn;
    int min, max;               /* the number of arguments; max -1: any */
};

static const char *err_file = "tl";
static int err_line;

/* the calls being made, for the messages */
#define MAXDEPTH 4000
static struct { const char *name, *file; int line; } calls[MAXDEPTH];
static int depth;

static void flush_all(void);

NORETURN static void die(const char *fmt, ...)
{
    va_list ap;
    flush_all();
    fprintf(stderr, "tl: %s:%d: ", err_file, err_line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    for (int i = depth - 1; i >= 0 && i >= depth - 10; i--)
        fprintf(stderr, "tl:   in %s, called at %s:%d\n", calls[i].name, calls[i].file, calls[i].line);
    exit(2);
}

/* ---- the lexer ---- */

enum {
    K_EOF, K_NAME, K_INT, K_FLT, K_STR, K_OP,
    K_VAR, K_FUNC, K_IF, K_ELSE, K_WHILE, K_DO, K_FOR, K_RETURN, K_BREAK, K_CONTINUE,
    K_NIL, K_TRUE, K_FALSE, K_INCLUDE, K_IN
};

static const char *const keywords[] = {
    "var", "func", "if", "else", "while", "do", "for", "return", "break", "continue",
    "nil", "true", "false", "include", "in", NULL
};

typedef struct Token {
    int k;
    char op[4];
    char *s;                    /* names, strings (malloc'd) */
    size_t slen;
    int64_t i;
    double f;
    int line;
} Token;

typedef struct Lexer {
    const char *file;
    char *src;
    size_t pos, len;
    int line;
    Token tok;
} Lexer;

NORETURN static void lex_error(Lexer *lx, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "tl: %s:%d: ", lx->file, lx->line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(2);
}

static int lx_peekc(Lexer *lx, size_t k)
{
    return lx->pos + k < lx->len ? (unsigned char)lx->src[lx->pos + k] : -1;
}

static void put_utf8(Str *b, unsigned long c)
{
    if (c < 0x80)
        str_putc(b, (int)c);
    else if (c < 0x800) {
        str_putc(b, (int)(0xc0 | (c >> 6)));
        str_putc(b, (int)(0x80 | (c & 0x3f)));
    } else if (c < 0x10000) {
        str_putc(b, (int)(0xe0 | (c >> 12)));
        str_putc(b, (int)(0x80 | ((c >> 6) & 0x3f)));
        str_putc(b, (int)(0x80 | (c & 0x3f)));
    } else {
        str_putc(b, (int)(0xf0 | (c >> 18)));
        str_putc(b, (int)(0x80 | ((c >> 12) & 0x3f)));
        str_putc(b, (int)(0x80 | ((c >> 6) & 0x3f)));
        str_putc(b, (int)(0x80 | (c & 0x3f)));
    }
}

/* an escape after the backslash, into b */
static void lex_escape(Lexer *lx, Str *b)
{
    int c = lx_peekc(lx, 0);
    if (c < 0)
        lex_error(lx, "unterminated escape");
    lx->pos++;
    switch (c) {
    case 'n': str_putc(b, '\n'); return;
    case 't': str_putc(b, '\t'); return;
    case 'r': str_putc(b, '\r'); return;
    case 'a': str_putc(b, '\a'); return;
    case 'b': str_putc(b, '\b'); return;
    case 'f': str_putc(b, '\f'); return;
    case 'v': str_putc(b, '\v'); return;
    case 'e': str_putc(b, 033); return;
    case '\\': case '"': case '\'': case '?': str_putc(b, c); return;
    case '\n': lx->line++; return;     /* a continued line */
    case 'x': {
        int v = 0, n = 0;
        while (n < 2 && isxdigit(lx_peekc(lx, 0))) {
            int d = lx_peekc(lx, 0);
            v = v * 16 + (isdigit(d) ? d - '0' : tolower(d) - 'a' + 10);
            lx->pos++;
            n++;
        }
        if (!n)
            lex_error(lx, "\\x without hexadecimal digits");
        str_putc(b, v);
        return;
    }
    case 'u': case 'U': {
        unsigned long v = 0;
        for (int n = c == 'u' ? 4 : 8; n > 0; n--) {
            int d = lx_peekc(lx, 0);
            if (!isxdigit(d))
                lex_error(lx, "\\%c needs %d hexadecimal digits", c, c == 'u' ? 4 : 8);
            v = v * 16 + (unsigned long)(isdigit(d) ? d - '0' : tolower(d) - 'a' + 10);
            lx->pos++;
        }
        put_utf8(b, v);
        return;
    }
    default:
        if (c >= '0' && c <= '7') {
            int v = c - '0', n = 1;
            while (n < 3 && lx_peekc(lx, 0) >= '0' && lx_peekc(lx, 0) <= '7') {
                v = v * 8 + lx_peekc(lx, 0) - '0';
                lx->pos++;
                n++;
            }
            str_putc(b, v & 0xff);
            return;
        }
        lex_error(lx, "unknown escape \\%c", c);
    }
}

static const char *const ops3[] = { "<<=", ">>=", "...", NULL };
static const char *const ops2[] = { "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=",
                                    "/=", "%=", "&=", "|=", "^=", "<<", ">>", NULL };

static void lex_next(Lexer *lx)
{
    Token *t = &lx->tok;
    free(t->s);
    t->s = NULL;
    for (;;) {
        int c = lx_peekc(lx, 0);
        if (c == '\n') {
            lx->line++;
            lx->pos++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            lx->pos++;
        } else if (c == '/' && lx_peekc(lx, 1) == '/') {
            while (lx_peekc(lx, 0) >= 0 && lx_peekc(lx, 0) != '\n')
                lx->pos++;
        } else if (c == '/' && lx_peekc(lx, 1) == '*') {
            lx->pos += 2;
            while (!(lx_peekc(lx, 0) == '*' && lx_peekc(lx, 1) == '/')) {
                if (lx_peekc(lx, 0) < 0)
                    lex_error(lx, "unterminated comment");
                if (lx_peekc(lx, 0) == '\n')
                    lx->line++;
                lx->pos++;
            }
            lx->pos += 2;
        } else
            break;
    }
    t->line = lx->line;
    int c = lx_peekc(lx, 0);
    if (c < 0) {
        t->k = K_EOF;
        return;
    }
    /* raw strings: R"delim( ... )delim" */
    if (c == 'R' && lx_peekc(lx, 1) == '"') {
        size_t p = lx->pos + 2, d = p;
        while (p < lx->len && lx->src[p] != '(' && p - d <= 16)
            p++;
        if (p >= lx->len || lx->src[p] != '(')
            lex_error(lx, "bad raw string delimiter");
        char delim[24];
        size_t dl = p - d;
        delim[0] = ')';
        memcpy(delim + 1, lx->src + d, dl);
        delim[dl + 1] = '"';
        size_t start = p + 1, q = start;
        for (;;) {
            if (q + dl + 2 > lx->len)
                lex_error(lx, "unterminated raw string");
            if (memcmp(lx->src + q, delim, dl + 2) == 0)
                break;
            q++;
        }
        for (size_t i = lx->pos; i < q; i++)
            if (lx->src[i] == '\n')
                lx->line++;
        t->k = K_STR;
        t->slen = q - start;
        t->s = xmalloc(t->slen + 1);
        memcpy(t->s, lx->src + start, t->slen);
        t->s[t->slen] = '\0';
        lx->pos = q + dl + 2;
        return;
    }
    if (isalpha(c) || c == '_') {
        size_t s = lx->pos;
        while (isalnum(lx_peekc(lx, 0)) || lx_peekc(lx, 0) == '_')
            lx->pos++;
        size_t n = lx->pos - s;
        t->s = xmalloc(n + 1);
        memcpy(t->s, lx->src + s, n);
        t->s[n] = '\0';
        t->slen = n;
        t->k = K_NAME;
        for (int i = 0; keywords[i]; i++)
            if (strcmp(keywords[i], t->s) == 0)
                t->k = K_VAR + i;
        return;
    }
    if (isdigit(c) || (c == '.' && isdigit(lx_peekc(lx, 1)))) {
        const char *s = lx->src + lx->pos;
        char *end;
        int isf = 0;
        if (c == '0' && (lx_peekc(lx, 1) == 'x' || lx_peekc(lx, 1) == 'X')) {
            t->i = (int64_t)strtoull(s, &end, 16);
        } else {
            size_t p = 0;
            while (isdigit((unsigned char)s[p]))
                p++;
            if (s[p] == '.' || s[p] == 'e' || s[p] == 'E')
                isf = 1;
            if (isf) {
                t->f = strtod(s, &end);
            } else if (c == '0') {
                t->i = (int64_t)strtoull(s, &end, 8);
            } else {
                t->i = (int64_t)strtoull(s, &end, 10);
            }
        }
        if (isalnum((unsigned char)*end) || *end == '_')
            lex_error(lx, "bad number");
        lx->pos = (size_t)(end - lx->src);
        t->k = isf ? K_FLT : K_INT;
        return;
    }
    if (c == '"') {
        Str *b = buf_new();
        lx->pos++;
        for (;;) {
            int d = lx_peekc(lx, 0);
            if (d < 0 || d == '\n')
                lex_error(lx, "unterminated string");
            lx->pos++;
            if (d == '"')
                break;
            if (d == '\\')
                lex_escape(lx, b);
            else
                str_putc(b, d);
        }
        t->k = K_STR;
        t->s = b->p;
        t->slen = b->len;
        free(b);
        return;
    }
    if (c == '\'') {
        Str *b = buf_new();
        lx->pos++;
        int d = lx_peekc(lx, 0);
        if (d == '\\') {
            lx->pos++;
            lex_escape(lx, b);
        } else if (d >= 0 && d != '\'' && d != '\n') {
            /* one character: its bytes (UTF-8) */
            lx->pos++;
            str_putc(b, d);
            while (lx_peekc(lx, 0) >= 0x80 && lx_peekc(lx, 0) < 0xc0) {
                str_putc(b, lx_peekc(lx, 0));
                lx->pos++;
            }
        }
        if (lx_peekc(lx, 0) != '\'' || b->len == 0)
            lex_error(lx, "bad character constant");
        lx->pos++;
        /* the value: the byte, or the code of a UTF-8 character */
        unsigned char *u = (unsigned char *)b->p;
        if (b->len == 1)
            t->i = u[0];
        else {
            int64_t v = u[0] & (0x3f >> (b->len - 2));
            for (size_t i = 1; i < b->len; i++)
                v = (v << 6) | (u[i] & 0x3f);
            t->i = v;
        }
        free(b->p);
        free(b);
        t->k = K_INT;
        return;
    }
    t->k = K_OP;
    for (int i = 0; ops3[i]; i++)
        if (strncmp(lx->src + lx->pos, ops3[i], 3) == 0) {
            strcpy(t->op, ops3[i]);
            lx->pos += 3;
            return;
        }
    for (int i = 0; ops2[i]; i++)
        if (strncmp(lx->src + lx->pos, ops2[i], 2) == 0) {
            strcpy(t->op, ops2[i]);
            lx->pos += 2;
            return;
        }
    if (!strchr("+-*/%<>=!~&|^?:;,.()[]{}", c))
        lex_error(lx, isprint(c) ? "unexpected character '%c'" : "unexpected byte \\%03o", c);
    t->op[0] = (char)c;
    t->op[1] = '\0';
    lx->pos++;
}

/* ---- the syntax tree ---- */

enum {
    /* expressions */
    N_LIT, N_LOCAL, N_GLOBAL, N_LISTX, N_MAPX, N_FUNCX, N_CALL, N_INDEX, N_UNARY, N_BINARY,
    N_AND, N_OR, N_COND, N_ASSIGN, N_INCDEC,
    /* statements */
    N_BLOCK, N_EXPR, N_LVAR, N_GVAR, N_IF, N_WHILE, N_DO, N_FOR, N_FOREACH, N_RETURN, N_BREAK,
    N_CONTINUE, N_NOP
};

/* the operators */
enum {
    O_ADD, O_SUB, O_MUL, O_DIV, O_MOD, O_SHL, O_SHR, O_BAND, O_BOR, O_BXOR, O_EQ, O_NE, O_LT,
    O_LE, O_GT, O_GE, O_IN, O_NEG, O_NOT, O_COMPL, O_PLUS, O_SET
};

struct Node {
    int k, op, line;
    const char *file;
    Node *a, *b, *c, *d;
    Node **kids;
    int nkids;
    Value lit;
    int slot;                   /* locals; globals */
    int post;                   /* N_INCDEC: postfix */
    Func *fn;
};

static Node *node(int k, int line, const char *file)
{
    Node *n = xcalloc(1, sizeof *n);
    n->k = k;
    n->line = line;
    n->file = file;
    return n;
}

static void add_kid(Node *n, Node *k)
{
    n->kids = xrealloc(n->kids, (size_t)(n->nkids + 1) * sizeof *n->kids);
    n->kids[n->nkids++] = k;
}

/* ---- the globals ---- */

typedef struct Global {
    char *name;
    Value v;
    int defined;
} Global;

static Global *globals;
static int nglobals, capglobals;

static int global_index(const char *name)
{
    for (int i = 0; i < nglobals; i++)
        if (strcmp(globals[i].name, name) == 0)
            return i;
    if (nglobals == capglobals) {
        capglobals = capglobals ? capglobals * 2 : 256;
        globals = xrealloc(globals, (size_t)capglobals * sizeof *globals);
    }
    globals[nglobals].name = xstrdup(name);
    globals[nglobals].v = NIL;
    globals[nglobals].defined = 0;
    return nglobals++;
}

static void global_define(const char *name, Value v)
{
    int i = global_index(name);
    decref(globals[i].v);
    globals[i].v = v;
    globals[i].defined = 1;
}

/* ---- the parser ---- */

typedef struct Scope {
    char **names;
    int *slots;
    int n, cap;
    int *marks;
    int nmarks, capmarks;
} Scope;

typedef struct FuncCtx {
    struct FuncCtx *outer;
    Func *fn;
    Scope sc;
    int nlocals;
} FuncCtx;

typedef struct Parser {
    Lexer *lx;
    FuncCtx *fc;                /* NULL: the top level */
    int loops;
    Node *top;                  /* the top-level statements */
} Parser;

NORETURN static void perr(Parser *p, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "tl: %s:%d: ", p->lx->file, p->lx->tok.line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(2);
}

static int is_op(Parser *p, const char *op)
{
    return p->lx->tok.k == K_OP && strcmp(p->lx->tok.op, op) == 0;
}

static void expect_op(Parser *p, const char *op)
{
    if (!is_op(p, op)) {
        Token *t = &p->lx->tok;
        if (t->k == K_EOF)
            perr(p, "expected '%s' at the end of the file", op);
        perr(p, "expected '%s' before %s%s%s", op, t->k == K_OP ? "'" : "", t->k == K_OP ? t->op :
             t->k == K_NAME ? t->s : t->k == K_STR ? "a string" : t->k == K_INT || t->k == K_FLT ?
             "a number" : t->s ? t->s : "this", t->k == K_OP ? "'" : "");
    }
    lex_next(p->lx);
}

static int accept_op(Parser *p, const char *op)
{
    if (is_op(p, op)) {
        lex_next(p->lx);
        return 1;
    }
    return 0;
}

static char *expect_name(Parser *p)
{
    if (p->lx->tok.k != K_NAME)
        perr(p, "expected a name");
    char *s = xstrdup(p->lx->tok.s);
    lex_next(p->lx);
    return s;
}

static void scope_push(FuncCtx *fc)
{
    Scope *s = &fc->sc;
    if (s->nmarks == s->capmarks) {
        s->capmarks = s->capmarks ? s->capmarks * 2 : 16;
        s->marks = xrealloc(s->marks, (size_t)s->capmarks * sizeof *s->marks);
    }
    s->marks[s->nmarks++] = s->n;
}

static void scope_pop(FuncCtx *fc)
{
    Scope *s = &fc->sc;
    int m = s->marks[--s->nmarks];
    while (s->n > m)
        free(s->names[--s->n]);
}

static int declare_local(Parser *p, const char *name)
{
    FuncCtx *fc = p->fc;
    Scope *s = &fc->sc;
    int m = s->nmarks ? s->marks[s->nmarks - 1] : 0;
    for (int i = m; i < s->n; i++)
        if (strcmp(s->names[i], name) == 0)
            perr(p, "%s declared twice", name);
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->names = xrealloc(s->names, (size_t)s->cap * sizeof *s->names);
        s->slots = xrealloc(s->slots, (size_t)s->cap * sizeof *s->slots);
    }
    s->names[s->n] = xstrdup(name);
    s->slots[s->n] = fc->nlocals;
    s->n++;
    return fc->nlocals++;
}

static int find_local(FuncCtx *fc, const char *name)
{
    for (int i = fc->sc.n - 1; i >= 0; i--)
        if (strcmp(fc->sc.names[i], name) == 0)
            return fc->sc.slots[i];
    return -1;
}

static Node *name_ref(Parser *p, const char *name, int line)
{
    Node *n;
    if (p->fc) {
        int s = find_local(p->fc, name);
        if (s >= 0) {
            n = node(N_LOCAL, line, p->lx->file);
            n->slot = s;
            return n;
        }
        for (FuncCtx *o = p->fc->outer; o; o = o->outer)
            if (find_local(o, name) >= 0)
                perr(p, "%s is a local of the enclosing function (functions do not see those)", name);
    }
    n = node(N_GLOBAL, line, p->lx->file);
    n->slot = global_index(name);
    return n;
}

static Node *expr(Parser *p);
static Node *assign_expr(Parser *p);
static Node *statement(Parser *p);
static Node *block(Parser *p);
static Func *func_rest(Parser *p, const char *name, int line);

static Node *lit_node(Parser *p, Value v)
{
    Node *n = node(N_LIT, p->lx->tok.line, p->lx->file);
    n->lit = v;
    return n;
}

static Node *primary(Parser *p)
{
    Token *t = &p->lx->tok;
    int line = t->line;
    Node *n;
    switch (t->k) {
    case K_INT:
        n = lit_node(p, vint(t->i));
        lex_next(p->lx);
        return n;
    case K_FLT:
        n = lit_node(p, vflt(t->f));
        lex_next(p->lx);
        return n;
    case K_STR: {
        Str *s = str_new(t->s, t->slen);
        lex_next(p->lx);
        while (t->k == K_STR) {     /* "a" "b" */
            str_append(s, t->s, t->slen);
            lex_next(p->lx);
        }
        n = node(N_LIT, line, p->lx->file);
        n->lit = vstr_take(s);
        return n;
    }
    case K_NIL:
        lex_next(p->lx);
        return lit_node(p, NIL);
    case K_TRUE:
        lex_next(p->lx);
        return lit_node(p, vint(1));
    case K_FALSE:
        lex_next(p->lx);
        return lit_node(p, vint(0));
    case K_NAME: {
        if (strcmp(t->s, "__FILE__") == 0) {
            n = lit_node(p, vstr(p->lx->file));
            lex_next(p->lx);
            return n;
        }
        if (strcmp(t->s, "__LINE__") == 0) {
            n = lit_node(p, vint(line));
            lex_next(p->lx);
            return n;
        }
        char *name = xstrdup(t->s);
        lex_next(p->lx);
        n = name_ref(p, name, line);
        free(name);
        return n;
    }
    case K_FUNC: {
        lex_next(p->lx);
        n = node(N_FUNCX, line, p->lx->file);
        n->fn = func_rest(p, "(anonymous function)", line);
        return n;
    }
    case K_OP:
        if (accept_op(p, "(")) {
            n = expr(p);
            expect_op(p, ")");
            return n;
        }
        if (accept_op(p, "[")) {
            n = node(N_LISTX, line, p->lx->file);
            while (!is_op(p, "]")) {
                add_kid(n, assign_expr(p));
                if (!accept_op(p, ","))
                    break;
            }
            expect_op(p, "]");
            return n;
        }
        if (accept_op(p, "{")) {
            n = node(N_MAPX, line, p->lx->file);
            while (!is_op(p, "}")) {
                Node *k;
                if (t->k == K_NAME || t->k >= K_VAR) {
                    /* name: value, a string key (a keyword too) */
                    k = lit_node(p, vstr(t->s));
                    lex_next(p->lx);
                } else
                    k = assign_expr(p);
                expect_op(p, ":");
                add_kid(n, k);
                add_kid(n, assign_expr(p));
                if (!accept_op(p, ","))
                    break;
            }
            expect_op(p, "}");
            return n;
        }
        break;
    default:
        break;
    }
    if (t->k == K_EOF)
        perr(p, "unexpected end of the file");
    perr(p, "unexpected %s%s%s", t->k == K_OP ? "'" : "", t->k == K_OP ? t->op : t->s ? t->s : "token",
         t->k == K_OP ? "'" : "");
    return NULL;
}

static int is_lvalue(Node *n)
{
    return n->k == N_LOCAL || n->k == N_GLOBAL || n->k == N_INDEX;
}

static Node *postfix(Parser *p)
{
    Node *n = primary(p);
    for (;;) {
        int line = p->lx->tok.line;
        if (accept_op(p, "(")) {
            Node *c = node(N_CALL, line, p->lx->file);
            c->a = n;
            while (!is_op(p, ")")) {
                add_kid(c, assign_expr(p));
                if (!accept_op(p, ","))
                    break;
            }
            expect_op(p, ")");
            n = c;
        } else if (accept_op(p, "[")) {
            Node *c = node(N_INDEX, line, p->lx->file);
            c->a = n;
            c->b = expr(p);
            expect_op(p, "]");
            n = c;
        } else if (accept_op(p, ".")) {
            Node *c = node(N_INDEX, line, p->lx->file);
            c->a = n;
            if (p->lx->tok.k >= K_VAR)      /* a keyword as a member name */
                p->lx->tok.k = K_NAME;
            char *name = expect_name(p);
            c->b = lit_node(p, vstr(name));
            free(name);
            n = c;
        } else if (is_op(p, "++") || is_op(p, "--")) {
            if (!is_lvalue(n))
                perr(p, "%s of something that cannot be assigned to", p->lx->tok.op);
            Node *c = node(N_INCDEC, line, p->lx->file);
            c->op = p->lx->tok.op[0] == '+' ? O_ADD : O_SUB;
            c->post = 1;
            c->a = n;
            lex_next(p->lx);
            n = c;
        } else
            return n;
    }
}

static Node *unary(Parser *p)
{
    int line = p->lx->tok.line;
    int op = -1;
    if (is_op(p, "-"))
        op = O_NEG;
    else if (is_op(p, "!"))
        op = O_NOT;
    else if (is_op(p, "~"))
        op = O_COMPL;
    else if (is_op(p, "+"))
        op = O_PLUS;
    if (op >= 0) {
        lex_next(p->lx);
        Node *n = node(N_UNARY, line, p->lx->file);
        n->op = op;
        n->a = unary(p);
        return n;
    }
    if (is_op(p, "++") || is_op(p, "--")) {
        Node *n = node(N_INCDEC, line, p->lx->file);
        n->op = p->lx->tok.op[0] == '+' ? O_ADD : O_SUB;
        lex_next(p->lx);
        n->a = unary(p);
        if (!is_lvalue(n->a))
            perr(p, "%s of something that cannot be assigned to", n->op == O_ADD ? "++" : "--");
        return n;
    }
    return postfix(p);
}

/* binary operators by precedence (lowest first) */
static const struct {
    const char *op;
    int code, prec;
} binops[] = {
    { "|", O_BOR, 3 }, { "^", O_BXOR, 4 }, { "&", O_BAND, 5 },
    { "==", O_EQ, 6 }, { "!=", O_NE, 6 },
    { "<", O_LT, 7 }, { "<=", O_LE, 7 }, { ">", O_GT, 7 }, { ">=", O_GE, 7 },
    { "<<", O_SHL, 8 }, { ">>", O_SHR, 8 },
    { "+", O_ADD, 9 }, { "-", O_SUB, 9 },
    { "*", O_MUL, 10 }, { "/", O_DIV, 10 }, { "%", O_MOD, 10 },
    { NULL, 0, 0 }
};

static Node *binary(Parser *p, int minprec)
{
    Node *l = unary(p);
    for (;;) {
        int code = -1, prec = 0;
        Token *t = &p->lx->tok;
        if (t->k == K_IN) {
            code = O_IN;
            prec = 7;
        } else if (t->k == K_OP) {
            for (int i = 0; binops[i].op; i++)
                if (strcmp(binops[i].op, t->op) == 0) {
                    code = binops[i].code;
                    prec = binops[i].prec;
                }
        }
        if (code < 0 || prec < minprec)
            return l;
        int line = t->line;
        lex_next(p->lx);
        Node *n = node(N_BINARY, line, p->lx->file);
        n->op = code;
        n->a = l;
        n->b = binary(p, prec + 1);
        l = n;
    }
}

static Node *and_expr(Parser *p)
{
    Node *l = binary(p, 3);
    while (is_op(p, "&&")) {
        Node *n = node(N_AND, p->lx->tok.line, p->lx->file);
        lex_next(p->lx);
        n->a = l;
        n->b = binary(p, 3);
        l = n;
    }
    return l;
}

static Node *or_expr(Parser *p)
{
    Node *l = and_expr(p);
    while (is_op(p, "||")) {
        Node *n = node(N_OR, p->lx->tok.line, p->lx->file);
        lex_next(p->lx);
        n->a = l;
        n->b = and_expr(p);
        l = n;
    }
    return l;
}

static Node *cond_expr(Parser *p)
{
    Node *c = or_expr(p);
    if (is_op(p, "?")) {
        Node *n = node(N_COND, p->lx->tok.line, p->lx->file);
        lex_next(p->lx);
        n->a = c;
        n->b = expr(p);
        expect_op(p, ":");
        n->c = cond_expr(p);
        return n;
    }
    return c;
}

static Node *assign_expr(Parser *p)
{
    Node *l = cond_expr(p);
    static const struct {
        const char *op;
        int code;
    } aops[] = {
        { "=", O_SET }, { "+=", O_ADD }, { "-=", O_SUB }, { "*=", O_MUL }, { "/=", O_DIV },
        { "%=", O_MOD }, { "<<=", O_SHL }, { ">>=", O_SHR }, { "&=", O_BAND }, { "|=", O_BOR },
        { "^=", O_BXOR }, { NULL, 0 }
    };
    if (p->lx->tok.k == K_OP)
        for (int i = 0; aops[i].op; i++)
            if (strcmp(aops[i].op, p->lx->tok.op) == 0) {
                if (!is_lvalue(l))
                    perr(p, "assignment to something that cannot be assigned to");
                Node *n = node(N_ASSIGN, p->lx->tok.line, p->lx->file);
                lex_next(p->lx);
                n->op = aops[i].code;
                n->a = l;
                n->b = assign_expr(p);
                return n;
            }
    return l;
}

static Node *expr(Parser *p)
{
    return assign_expr(p);
}

/* "(params) { body }" of a function */
static Func *func_rest(Parser *p, const char *name, int line)
{
    Func *f = xcalloc(1, sizeof *f);
    f->name = xstrdup(name);
    f->file = p->lx->file;
    f->line = line;
    FuncCtx fc;
    memset(&fc, 0, sizeof fc);
    fc.outer = p->fc;
    fc.fn = f;
    FuncCtx *saved = p->fc;
    int savedloops = p->loops;
    p->fc = &fc;
    p->loops = 0;
    scope_push(&fc);
    expect_op(p, "(");
    while (!is_op(p, ")")) {
        char *pn = expect_name(p);
        declare_local(p, pn);
        free(pn);
        f->nparams++;
        if (!accept_op(p, ","))
            break;
    }
    expect_op(p, ")");
    if (!is_op(p, "{"))
        perr(p, "expected the body of the function");
    f->body = block(p);
    scope_pop(&fc);
    f->nlocals = fc.nlocals;
    free(fc.sc.names);
    free(fc.sc.slots);
    free(fc.sc.marks);
    p->fc = saved;
    p->loops = savedloops;
    return f;
}

static Node *block(Parser *p)
{
    Node *n = node(N_BLOCK, p->lx->tok.line, p->lx->file);
    expect_op(p, "{");
    if (p->fc)
        scope_push(p->fc);
    while (!is_op(p, "}")) {
        if (p->lx->tok.k == K_EOF)
            perr(p, "missing '}'");
        add_kid(n, statement(p));
    }
    lex_next(p->lx);
    if (p->fc)
        scope_pop(p->fc);
    return n;
}

static Node *var_decl(Parser *p)
{
    /* var a = 1, b; */
    Node *blk = node(N_BLOCK, p->lx->tok.line, p->lx->file);
    do {
        int line = p->lx->tok.line;
        char *name = expect_name(p);
        Node *init = NULL;
        if (accept_op(p, "="))
            init = assign_expr(p);
        Node *n;
        if (p->fc) {
            n = node(N_LVAR, line, p->lx->file);
            n->slot = declare_local(p, name);   /* after the initializer: var x = x is the outer x */
        } else {
            n = node(N_GVAR, line, p->lx->file);
            n->slot = global_index(name);
        }
        n->a = init;
        add_kid(blk, n);
        free(name);
    } while (accept_op(p, ","));
    return blk;
}

static Node *parse_file(Parser *p, const char *path);

static Node *statement(Parser *p)
{
    Token *t = &p->lx->tok;
    int line = t->line;
    Node *n;
    switch (t->k) {
    case K_OP:
        if (is_op(p, "{"))
            return block(p);
        if (accept_op(p, ";"))
            return node(N_NOP, line, p->lx->file);
        break;
    case K_VAR:
        lex_next(p->lx);
        n = var_decl(p);
        expect_op(p, ";");
        return n;
    case K_FUNC: {
        /* func name(...) {...}: a definition; func (...) is an expression */
        Lexer save = *p->lx;
        char *saves = p->lx->tok.s ? xstrdup(p->lx->tok.s) : NULL;
        lex_next(p->lx);
        if (t->k == K_NAME) {
            if (p->fc)
                perr(p, "functions are defined at the top level");
            char *name = expect_name(p);
            Func *f = func_rest(p, name, line);
            Value v;
            v.t = T_FUNC;
            v.u.fn = f;
            global_define(name, v);
            free(name);
            free(saves);
            return node(N_NOP, line, p->lx->file);
        }
        /* back: an expression statement */
        free(p->lx->tok.s);
        *p->lx = save;
        p->lx->tok.s = saves;
        break;
    }
    case K_IF:
        lex_next(p->lx);
        n = node(N_IF, line, p->lx->file);
        expect_op(p, "(");
        n->a = expr(p);
        expect_op(p, ")");
        n->b = statement(p);
        if (t->k == K_ELSE) {
            lex_next(p->lx);
            n->c = statement(p);
        }
        return n;
    case K_WHILE:
        lex_next(p->lx);
        n = node(N_WHILE, line, p->lx->file);
        expect_op(p, "(");
        n->a = expr(p);
        expect_op(p, ")");
        p->loops++;
        n->b = statement(p);
        p->loops--;
        return n;
    case K_DO:
        lex_next(p->lx);
        n = node(N_DO, line, p->lx->file);
        p->loops++;
        n->b = statement(p);
        p->loops--;
        if (t->k != K_WHILE)
            perr(p, "expected while after do");
        lex_next(p->lx);
        expect_op(p, "(");
        n->a = expr(p);
        expect_op(p, ")");
        expect_op(p, ";");
        return n;
    case K_FOR: {
        lex_next(p->lx);
        expect_op(p, "(");
        if (p->fc)
            scope_push(p->fc);
        /* for (var x : list) */
        if (t->k == K_VAR) {
            Lexer save = *p->lx;
            char *saves = xstrdup(t->s ? t->s : "");
            lex_next(p->lx);
            if (t->k == K_NAME) {
                char *name = xstrdup(t->s);
                lex_next(p->lx);
                if (is_op(p, ":")) {
                    lex_next(p->lx);
                    n = node(N_FOREACH, line, p->lx->file);
                    n->b = expr(p);
                    expect_op(p, ")");
                    if (p->fc) {
                        n->a = node(N_LOCAL, line, p->lx->file);
                        n->a->slot = declare_local(p, name);
                    } else {
                        n->a = node(N_GLOBAL, line, p->lx->file);
                        n->a->slot = global_index(name);
                        globals[n->a->slot].defined = 1;
                    }
                    free(name);
                    free(saves);
                    p->loops++;
                    n->c = statement(p);
                    p->loops--;
                    if (p->fc)
                        scope_pop(p->fc);
                    return n;
                }
                free(name);
            }
            free(p->lx->tok.s);
            *p->lx = save;
            p->lx->tok.s = saves;
        }
        n = node(N_FOR, line, p->lx->file);
        if (t->k == K_VAR) {
            lex_next(p->lx);
            n->a = var_decl(p);
        } else if (!is_op(p, ";")) {
            Node *e = node(N_BLOCK, line, p->lx->file);
            do {
                Node *s = node(N_EXPR, line, p->lx->file);
                s->a = expr(p);
                add_kid(e, s);
            } while (accept_op(p, ","));
            n->a = e;
        }
        expect_op(p, ";");
        if (!is_op(p, ";"))
            n->b = expr(p);
        expect_op(p, ";");
        if (!is_op(p, ")")) {
            Node *e = node(N_BLOCK, line, p->lx->file);
            do {
                Node *s = node(N_EXPR, line, p->lx->file);
                s->a = expr(p);
                add_kid(e, s);
            } while (accept_op(p, ","));
            n->c = e;
        }
        expect_op(p, ")");
        p->loops++;
        n->d = statement(p);
        p->loops--;
        if (p->fc)
            scope_pop(p->fc);
        return n;
    }
    case K_RETURN:
        if (!p->fc)
            perr(p, "return outside a function (exit() ends the script)");
        lex_next(p->lx);
        n = node(N_RETURN, line, p->lx->file);
        if (!is_op(p, ";"))
            n->a = expr(p);
        expect_op(p, ";");
        return n;
    case K_BREAK:
    case K_CONTINUE:
        if (!p->loops)
            perr(p, "%s outside a loop", t->k == K_BREAK ? "break" : "continue");
        n = node(t->k == K_BREAK ? N_BREAK : N_CONTINUE, line, p->lx->file);
        lex_next(p->lx);
        expect_op(p, ";");
        return n;
    case K_INCLUDE: {
        if (p->fc)
            perr(p, "include is for the top level");
        lex_next(p->lx);
        if (t->k != K_STR)
            perr(p, "include needs a string");
        char *rel = xstrdup(t->s);
        lex_next(p->lx);
        expect_op(p, ";");
        /* relative to the including file's directory */
        char *path;
        if (rel[0] == '/')
            path = rel;
        else {
            const char *slash = strrchr(p->lx->file, '/');
            size_t dl = slash ? (size_t)(slash - p->lx->file) + 1 : 0;
            path = xmalloc(dl + strlen(rel) + 1);
            memcpy(path, p->lx->file, dl);
            strcpy(path + dl, rel);
            free(rel);
        }
        /* each file once */
        static char **seen;
        static int nseen;
        char *real = realpath(path, NULL);
        const char *key = real ? real : path;
        for (int i = 0; i < nseen; i++)
            if (strcmp(seen[i], key) == 0) {
                free(real);
                return node(N_NOP, line, p->lx->file);
            }
        seen = xrealloc(seen, (size_t)(nseen + 1) * sizeof *seen);
        seen[nseen++] = xstrdup(key);
        free(real);
        Lexer *saved = p->lx;
        Node *b = parse_file(p, path);
        p->lx = saved;
        return b;
    }
    default:
        break;
    }
    n = node(N_EXPR, line, p->lx->file);
    n->a = expr(p);
    expect_op(p, ";");
    return n;
}

static char *read_all_fd(int fd, size_t *len)
{
    size_t cap = 65536, n = 0;
    char *b = xmalloc(cap);
    for (;;) {
        if (n == cap) {
            cap *= 2;
            b = xrealloc(b, cap);
        }
        ssize_t r = read(fd, b + n, cap - n);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        n += (size_t)r;
    }
    *len = n;
    return b;
}

static Node *parse_source(Parser *p, const char *file, char *src, size_t len)
{
    Lexer *lx = xcalloc(1, sizeof *lx);
    lx->file = file;
    lx->src = src;
    lx->len = len;
    lx->line = 1;
    if (len >= 2 && src[0] == '#' && src[1] == '!')
        while (lx->pos < len && src[lx->pos] != '\n')
            lx->pos++;
    p->lx = lx;
    lex_next(lx);
    Node *b = node(N_BLOCK, 1, file);
    while (lx->tok.k != K_EOF)
        add_kid(b, statement(p));
    return b;
}

static Node *parse_file(Parser *p, const char *path)
{
    int fd = strcmp(path, "-") == 0 ? 0 : open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "tl: %s: %s\n", path, strerror(errno));
        exit(2);
    }
    size_t len;
    char *src = read_all_fd(fd, &len);
    if (fd)
        close(fd);
    return parse_source(p, path, src, len);
}

/* ---- conversions ---- */

static const char *type_name(int t)
{
    static const char *const names[] = { "nil", "int", "double", "string", "list", "map", "func",
                                         "func", "term" };
    return names[t];
}

static int truth(Value v)
{
    switch (v.t) {
    case T_NIL: return 0;
    case T_INT: return v.u.i != 0;
    case T_FLT: return v.u.f != 0;
    case T_STR: return v.u.s->len != 0;
    default: return 1;
    }
}

static void repr_into(Str *b, Value v, int quote);

static void quote_into(Str *b, const char *p, size_t n)
{
    str_putc(b, '"');
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"': str_append(b, "\\\"", 2); break;
        case '\\': str_append(b, "\\\\", 2); break;
        case '\n': str_append(b, "\\n", 2); break;
        case '\t': str_append(b, "\\t", 2); break;
        case '\r': str_append(b, "\\r", 2); break;
        case '\a': str_append(b, "\\a", 2); break;
        case '\b': str_append(b, "\\b", 2); break;
        case '\f': str_append(b, "\\f", 2); break;
        case '\v': str_append(b, "\\v", 2); break;
        default:
            if (c < 0x20 || c == 0x7f)
                buf_printf(b, "\\%03o", c);
            else
                str_putc(b, c);
        }
    }
    str_putc(b, '"');
}

static void fmt_double(Str *b, double f)
{
    char tmp[64];
    if (f == dfloor(f) && dfabs(f) < 1e15) {
        snprintf(tmp, sizeof tmp, "%.1f", f);
    } else {
        snprintf(tmp, sizeof tmp, "%.17g", f);
        /* the shortest that reads back the same */
        for (int prec = 1; prec <= 17; prec++) {
            char t2[64];
            snprintf(t2, sizeof t2, "%.*g", prec, f);
            if (strtod(t2, NULL) == f) {
                strcpy(tmp, t2);
                break;
            }
        }
    }
    str_append(b, tmp, strlen(tmp));
}

static void repr_into(Str *b, Value v, int quote)
{
    switch (v.t) {
    case T_NIL: str_append(b, "nil", 3); break;
    case T_INT: buf_printf(b, "%lld", (long long)v.u.i); break;
    case T_FLT: fmt_double(b, v.u.f); break;
    case T_STR:
        if (quote)
            quote_into(b, v.u.s->p, v.u.s->len);
        else
            str_append(b, v.u.s->p, v.u.s->len);
        break;
    case T_LIST:
        str_putc(b, '[');
        for (size_t i = 0; i < v.u.l->n; i++) {
            if (i)
                str_append(b, ", ", 2);
            repr_into(b, v.u.l->v[i], 1);
        }
        str_putc(b, ']');
        break;
    case T_MAP:
        str_putc(b, '{');
        for (size_t i = 0; i < v.u.m->n; i++) {
            if (i)
                str_append(b, ", ", 2);
            repr_into(b, v.u.m->k[i], 1);
            str_append(b, ": ", 2);
            repr_into(b, v.u.m->v[i], 1);
        }
        str_putc(b, '}');
        break;
    case T_FUNC: buf_printf(b, "<func %s>", v.u.fn->name); break;
    case T_BUILTIN: buf_printf(b, "<builtin %s>", v.u.b->name); break;
    case T_TERM: str_append(b, "<term>", 6); break;
    }
}

/* the value as a string (a new value) */
static Value to_str(Value v)
{
    if (v.t == T_STR)
        return incref(v);
    Str *b = buf_new();
    repr_into(b, v, 0);
    return vstr_take(b);
}

static int values_equal_(Value a, Value b);

/* ==: by value; a list or map is equal to itself at once (so that one in a
   cycle can be compared with itself), and cycles elsewhere end in an error */
static int values_equal(Value a, Value b)
{
    static int eqdepth;
    if (a.t == b.t && ((a.t == T_LIST && a.u.l == b.u.l) || (a.t == T_MAP && a.u.m == b.u.m)))
        return 1;
    if (++eqdepth > 10000) {
        eqdepth = 0;
        die("== of values nested too deeply (a cycle?)");
    }
    int r = values_equal_(a, b);
    eqdepth--;
    return r;
}

static int values_equal_(Value a, Value b)
{
    if ((a.t == T_INT || a.t == T_FLT) && (b.t == T_INT || b.t == T_FLT)) {
        if (a.t == T_INT && b.t == T_INT)
            return a.u.i == b.u.i;
        return (a.t == T_INT ? (double)a.u.i : a.u.f) == (b.t == T_INT ? (double)b.u.i : b.u.f);
    }
    if (a.t != b.t)
        return 0;
    switch (a.t) {
    case T_NIL: return 1;
    case T_STR: return a.u.s->len == b.u.s->len && memcmp(a.u.s->p, b.u.s->p, a.u.s->len) == 0;
    case T_LIST:
        if (a.u.l->n != b.u.l->n)
            return 0;
        for (size_t i = 0; i < a.u.l->n; i++)
            if (!values_equal(a.u.l->v[i], b.u.l->v[i]))
                return 0;
        return 1;
    case T_MAP:
        if (a.u.m->n != b.u.m->n)
            return 0;
        for (size_t i = 0; i < a.u.m->n; i++) {
            long e = map_find(b.u.m, a.u.m->k[i]);
            if (e < 0 || !values_equal(a.u.m->v[i], b.u.m->v[e]))
                return 0;
        }
        return 1;
    case T_FUNC: return a.u.fn == b.u.fn;
    case T_BUILTIN: return a.u.b == b.u.b;
    case T_TERM: return a.u.term == b.u.term;
    }
    return 0;
}

/* <0, 0, >0: numbers by value, strings by bytes, lists element by element */
static int compare(Value a, Value b)
{
    if ((a.t == T_INT || a.t == T_FLT) && (b.t == T_INT || b.t == T_FLT)) {
        if (a.t == T_INT && b.t == T_INT)
            return a.u.i < b.u.i ? -1 : a.u.i > b.u.i;
        double x = a.t == T_INT ? (double)a.u.i : a.u.f, y = b.t == T_INT ? (double)b.u.i : b.u.f;
        return x < y ? -1 : x > y;
    }
    if (a.t == T_STR && b.t == T_STR) {
        size_t n = a.u.s->len < b.u.s->len ? a.u.s->len : b.u.s->len;
        int c = memcmp(a.u.s->p, b.u.s->p, n);
        if (c)
            return c < 0 ? -1 : 1;
        return a.u.s->len < b.u.s->len ? -1 : a.u.s->len > b.u.s->len;
    }
    if (a.t == T_LIST && b.t == T_LIST) {
        for (size_t i = 0; i < a.u.l->n && i < b.u.l->n; i++) {
            int c = compare(a.u.l->v[i], b.u.l->v[i]);
            if (c)
                return c;
        }
        return a.u.l->n < b.u.l->n ? -1 : a.u.l->n > b.u.l->n;
    }
    if (a.t == T_NIL && b.t == T_NIL)
        return 0;
    die("cannot compare %s and %s", type_name(a.t), type_name(b.t));
    return 0;
}

static int64_t need_int(Value v, const char *what)
{
    if (v.t == T_INT)
        return v.u.i;
    if (v.t == T_FLT)
        return (int64_t)v.u.f;
    die("%s: an integer expected, not %s", what, type_name(v.t));
    return 0;
}

static double need_num(Value v, const char *what)
{
    if (v.t == T_INT)
        return (double)v.u.i;
    if (v.t == T_FLT)
        return v.u.f;
    die("%s: a number expected, not %s", what, type_name(v.t));
    return 0;
}

static Str *need_str(Value v, const char *what)
{
    if (v.t != T_STR)
        die("%s: a string expected, not %s", what, type_name(v.t));
    return v.u.s;
}

static List *need_list(Value v, const char *what)
{
    if (v.t != T_LIST)
        die("%s: a list expected, not %s", what, type_name(v.t));
    return v.u.l;
}

static Map *need_map(Value v, const char *what)
{
    if (v.t != T_MAP)
        die("%s: a map expected, not %s", what, type_name(v.t));
    return v.u.m;
}

/* ---- printf ---- */

static void format_into(Str *b, const char *fmt, size_t flen, Value *a, int n)
{
    int ai = 0;
    for (size_t i = 0; i < flen; i++) {
        char c = fmt[i];
        if (c != '%') {
            str_putc(b, c);
            continue;
        }
        if (i + 1 < flen && fmt[i + 1] == '%') {
            str_putc(b, '%');
            i++;
            continue;
        }
        char spec[32];
        size_t sl = 0;
        spec[sl++] = '%';
        i++;
        while (i < flen && strchr("-+ #0", fmt[i]) && sl < 10)
            spec[sl++] = fmt[i++];
        int width = -1, prec = -1;
        if (i < flen && fmt[i] == '*') {
            if (ai >= n)
                die("printf: too few arguments");
            width = (int)need_int(a[ai++], "printf width");
            i++;
        } else
            while (i < flen && isdigit((unsigned char)fmt[i])) {
                width = (width < 0 ? 0 : width) * 10 + fmt[i] - '0';
                i++;
            }
        if (i < flen && fmt[i] == '.') {
            i++;
            prec = 0;
            if (i < flen && fmt[i] == '*') {
                if (ai >= n)
                    die("printf: too few arguments");
                prec = (int)need_int(a[ai++], "printf precision");
                i++;
            } else
                while (i < flen && isdigit((unsigned char)fmt[i])) {
                    prec = prec * 10 + fmt[i] - '0';
                    i++;
                }
        }
        /* C's length modifiers are accepted and ignored */
        while (i < flen && strchr("hlLjzt", fmt[i]))
            i++;
        if (i >= flen)
            die("printf: the format ends in a conversion");
        char conv = fmt[i];
        if (width >= 0)
            sl += (size_t)snprintf(spec + sl, sizeof spec - sl, "%d", width);
        if (prec >= 0)
            sl += (size_t)snprintf(spec + sl, sizeof spec - sl, ".%d", prec);
        if (ai >= n)
            die("printf: too few arguments for the format");
        Value v = a[ai++];
        switch (conv) {
        case 'd': case 'i':
            strcpy(spec + sl, "lld");
            buf_printf(b, spec, (long long)need_int(v, "printf %d"));
            break;
        case 'u': case 'o': case 'x': case 'X':
            spec[sl] = 'l';
            spec[sl + 1] = 'l';
            spec[sl + 2] = conv;
            spec[sl + 3] = '\0';
            buf_printf(b, spec, (unsigned long long)need_int(v, "printf"));
            break;
        case 'c':
            if (v.t == T_STR) {
                strcpy(spec + sl, "s");
                buf_printf(b, spec, v.u.s->p);
            } else {
                /* a byte (<256), or a character in UTF-8 */
                int64_t ch = need_int(v, "printf %c");
                if (ch < 256)
                    str_putc(b, (int)ch);
                else
                    put_utf8(b, (unsigned long)ch);
            }
            break;
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a': case 'A':
            spec[sl] = conv;
            spec[sl + 1] = '\0';
            buf_printf(b, spec, need_num(v, "printf"));
            break;
        case 's': case 'q': {
            Str *t = buf_new();
            if (conv == 'q')
                repr_into(t, v, 1);
            else
                repr_into(t, v, 0);
            /* width and precision in bytes; the string may hold NULs */
            size_t len = t->len;
            if (prec >= 0 && (size_t)prec < len)
                len = (size_t)prec;
            int left = strchr(spec, '-') != NULL;
            if (!left)
                for (int k = (int)len; k < width; k++)
                    str_putc(b, ' ');
            str_append(b, t->p, len);
            if (left)
                for (int k = (int)len; k < width; k++)
                    str_putc(b, ' ');
            free(t->p);
            free(t);
            break;
        }
        default:
            die("printf: unknown conversion %%%c", conv);
        }
    }
    if (ai < n)
        die("printf: more arguments than the format uses");
}

/* ---- evaluation ---- */

enum { EX_NORMAL, EX_BREAK, EX_CONTINUE, EX_RETURN };
static Value retval;

static Value eval(Node *n, Value *L);
static int exec(Node *n, Value *L);
static Value call_value(Value f, Value *args, int nargs);

static Value arith(int op, Value a, Value b)
{
    if (op == O_ADD) {
        if (a.t == T_STR && b.t == T_STR) {
            Str *s = str_new(a.u.s->p, a.u.s->len);
            str_append(s, b.u.s->p, b.u.s->len);
            return vstr_take(s);
        }
        if (a.t == T_LIST && b.t == T_LIST) {
            List *l = list_new(a.u.l->n + b.u.l->n);
            for (size_t i = 0; i < a.u.l->n; i++)
                list_push(l, incref(a.u.l->v[i]));
            for (size_t i = 0; i < b.u.l->n; i++)
                list_push(l, incref(b.u.l->v[i]));
            return vlist(l);
        }
        if (a.t == T_STR || b.t == T_STR)
            die("+ of %s and %s (str() or sprintf() make strings)", type_name(a.t), type_name(b.t));
    }
    if (op == O_MUL && ((a.t == T_STR && b.t == T_INT) || (a.t == T_INT && b.t == T_STR))) {
        Str *s = a.t == T_STR ? a.u.s : b.u.s;
        int64_t k = a.t == T_INT ? a.u.i : b.u.i;
        Str *r = buf_new();
        for (int64_t i = 0; i < k; i++)
            str_append(r, s->p, s->len);
        return vstr_take(r);
    }
    if (op == O_MUL && ((a.t == T_LIST && b.t == T_INT) || (a.t == T_INT && b.t == T_LIST))) {
        List *s = a.t == T_LIST ? a.u.l : b.u.l;
        int64_t k = a.t == T_INT ? a.u.i : b.u.i;
        List *r = list_new(k > 0 ? (size_t)k * s->n : 0);
        for (int64_t i = 0; i < k; i++)
            for (size_t j = 0; j < s->n; j++)
                list_push(r, incref(s->v[j]));
        return vlist(r);
    }
    static const char *const names[] = { "+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^" };
    if ((a.t != T_INT && a.t != T_FLT) || (b.t != T_INT && b.t != T_FLT))
        die("%s of %s and %s", names[op], type_name(a.t), type_name(b.t));
    if (a.t == T_INT && b.t == T_INT) {
        int64_t x = a.u.i, y = b.u.i;
        switch (op) {
        case O_ADD: return vint((int64_t)((uint64_t)x + (uint64_t)y));
        case O_SUB: return vint((int64_t)((uint64_t)x - (uint64_t)y));
        case O_MUL: return vint((int64_t)((uint64_t)x * (uint64_t)y));
        case O_DIV:
            if (!y)
                die("division by zero");
            if (x == INT64_MIN && y == -1)
                return vint(x);
            return vint(x / y);
        case O_MOD:
            if (!y)
                die("division by zero");
            if (y == -1)
                return vint(0);
            return vint(x % y);
        case O_SHL: return vint(y >= 64 || y < 0 ? 0 : (int64_t)((uint64_t)x << y));
        case O_SHR: return vint(y >= 64 || y < 0 ? (x < 0 ? -1 : 0) : x >> y);
        case O_BAND: return vint(x & y);
        case O_BOR: return vint(x | y);
        case O_BXOR: return vint(x ^ y);
        }
    }
    double x = a.t == T_INT ? (double)a.u.i : a.u.f, y = b.t == T_INT ? (double)b.u.i : b.u.f;
    switch (op) {
    case O_ADD: return vflt(x + y);
    case O_SUB: return vflt(x - y);
    case O_MUL: return vflt(x * y);
    case O_DIV: return vflt(x / y);
    case O_MOD: return vflt(dfmod(x, y));
    default: die("%s of doubles", names[op]);
    }
    return NIL;
}

static Value binop(int op, Value a, Value b)
{
    switch (op) {
    case O_EQ: return vint(values_equal(a, b));
    case O_NE: return vint(!values_equal(a, b));
    case O_LT: return vint(compare(a, b) < 0);
    case O_LE: return vint(compare(a, b) <= 0);
    case O_GT: return vint(compare(a, b) > 0);
    case O_GE: return vint(compare(a, b) >= 0);
    case O_IN:
        if (b.t == T_MAP)
            return vint(map_find(b.u.m, a) >= 0);
        if (b.t == T_LIST) {
            for (size_t i = 0; i < b.u.l->n; i++)
                if (values_equal(a, b.u.l->v[i]))
                    return vint(1);
            return vint(0);
        }
        if (b.t == T_STR) {
            Str *s = need_str(a, "in (a substring)"), *t = b.u.s;
            if (s->len == 0)
                return vint(1);
            for (size_t i = 0; i + s->len <= t->len; i++)
                if (memcmp(t->p + i, s->p, s->len) == 0)
                    return vint(1);
            return vint(0);
        }
        die("in of %s", type_name(b.t));
        return NIL;
    default:
        return arith(op, a, b);
    }
}

/* the value at a[k] (new reference); nil for a map without k */
static Value index_get(Value a, Value k)
{
    switch (a.t) {
    case T_LIST: {
        int64_t i = need_int(k, "list index");
        if (i < 0 || (size_t)i >= a.u.l->n)
            die("list index %lld out of range (the length is %zu)", (long long)i, a.u.l->n);
        return incref(a.u.l->v[i]);
    }
    case T_MAP:
        if (k.t != T_STR && k.t != T_INT)
            die("a map key is a string or an integer, not %s", type_name(k.t));
        return incref(map_get(a.u.m, k));
    case T_STR: {
        int64_t i = need_int(k, "string index");
        if (i < 0 || (size_t)i >= a.u.s->len)
            die("string index %lld out of range (the length is %zu)", (long long)i, a.u.s->len);
        return vint((unsigned char)a.u.s->p[i]);
    }
    default:
        die("indexing %s", type_name(a.t));
    }
    return NIL;
}

/* takes v */
static void index_set(Value a, Value k, Value v)
{
    switch (a.t) {
    case T_LIST: {
        int64_t i = need_int(k, "list index");
        if (i < 0 || (size_t)i >= a.u.l->n)
            die("list index %lld out of range (the length is %zu)", (long long)i, a.u.l->n);
        decref(a.u.l->v[i]);
        a.u.l->v[i] = v;
        return;
    }
    case T_MAP:
        if (k.t != T_STR && k.t != T_INT)
            die("a map key is a string or an integer, not %s", type_name(k.t));
        map_set(a.u.m, incref(k), v);
        return;
    case T_STR:
        die("strings cannot be changed (make a new one)");
        return;
    default:
        die("indexing %s", type_name(a.t));
    }
}

static Value *global_slot(Node *n, int forwrite)
{
    Global *g = &globals[n->slot];
    if (!g->defined) {
        err_file = n->file;
        err_line = n->line;
        die(forwrite ? "%s is not declared (var %s)" : "%s is not defined", g->name, g->name);
    }
    return &g->v;
}

/* assigns (op= too) to an lvalue; returns the new value (new reference) */
static Value assign(Node *lv, int op, Value rhs, Value *L)
{
    Value *slot = NULL;
    if (lv->k == N_LOCAL)
        slot = &L[lv->slot];
    else if (lv->k == N_GLOBAL)
        slot = global_slot(lv, 1);
    if (slot) {
        if (op == O_SET) {
            decref(*slot);
            *slot = rhs;
            return incref(rhs);
        }
        if (op == O_ADD && slot->t == T_STR && rhs.t == T_STR && slot->u.s->rc == 1) {
            /* s += t, in place */
            str_append(slot->u.s, rhs.u.s->p, rhs.u.s->len);
            decref(rhs);
            return incref(*slot);
        }
        Value nv = binop(op, *slot, rhs);
        decref(rhs);
        decref(*slot);
        *slot = nv;
        return incref(nv);
    }
    /* a[k] */
    Value a = eval(lv->a, L);
    Value k = eval(lv->b, L);
    err_line = lv->line;
    err_file = lv->file;
    Value nv;
    if (op == O_SET)
        nv = rhs;
    else {
        Value old = index_get(a, k);
        nv = binop(op, old, rhs);
        decref(old);
        decref(rhs);
    }
    index_set(a, k, incref(nv));
    decref(a);
    decref(k);
    return nv;
}

static Value lvalue_get(Node *lv, Value *L)
{
    if (lv->k == N_LOCAL)
        return incref(L[lv->slot]);
    if (lv->k == N_GLOBAL)
        return incref(*global_slot(lv, 0));
    return eval(lv, L);
}

static Value eval(Node *n, Value *L)
{
    switch (n->k) {
    case N_LIT:
        return incref(n->lit);
    case N_LOCAL:
        return incref(L[n->slot]);
    case N_GLOBAL:
        return incref(*global_slot(n, 0));
    case N_LISTX: {
        List *l = list_new((size_t)n->nkids);
        for (int i = 0; i < n->nkids; i++)
            list_push(l, eval(n->kids[i], L));
        return vlist(l);
    }
    case N_MAPX: {
        Map *m = map_new();
        Value mv = vmap(m);
        for (int i = 0; i < n->nkids; i += 2) {
            Value k = eval(n->kids[i], L);
            if (k.t != T_STR && k.t != T_INT) {
                err_line = n->line;
                die("a map key is a string or an integer, not %s", type_name(k.t));
            }
            map_set(m, k, eval(n->kids[i + 1], L));
        }
        return mv;
    }
    case N_FUNCX: {
        Value v;
        v.t = T_FUNC;
        v.u.fn = n->fn;
        return v;
    }
    case N_CALL: {
        Value f = eval(n->a, L);
        int na = n->nkids;
        Value argsbuf[8];
        Value *args = na <= 8 ? argsbuf : xmalloc((size_t)na * sizeof *args);
        for (int i = 0; i < na; i++)
            args[i] = eval(n->kids[i], L);
        err_line = n->line;
        err_file = n->file;
        Value r = call_value(f, args, na);
        for (int i = 0; i < na; i++)
            decref(args[i]);
        if (args != argsbuf)
            free(args);
        decref(f);
        return r;
    }
    case N_INDEX: {
        Value a = eval(n->a, L);
        Value k = eval(n->b, L);
        err_line = n->line;
        err_file = n->file;
        Value r = index_get(a, k);
        decref(a);
        decref(k);
        return r;
    }
    case N_UNARY: {
        Value a = eval(n->a, L);
        err_line = n->line;
        err_file = n->file;
        Value r = NIL;
        switch (n->op) {
        case O_NOT: r = vint(!truth(a)); break;
        case O_NEG:
            if (a.t == T_INT)
                r = vint((int64_t)(0 - (uint64_t)a.u.i));
            else
                r = vflt(-need_num(a, "-"));
            break;
        case O_PLUS: r = a.t == T_INT ? vint(a.u.i) : vflt(need_num(a, "+")); break;
        case O_COMPL: r = vint(~need_int(a, "~")); break;
        }
        decref(a);
        return r;
    }
    case N_BINARY: {
        Value a = eval(n->a, L);
        Value b = eval(n->b, L);
        err_line = n->line;
        err_file = n->file;
        Value r = binop(n->op, a, b);
        decref(a);
        decref(b);
        return r;
    }
    case N_AND: {
        Value a = eval(n->a, L);
        if (!truth(a))
            return a;
        decref(a);
        return eval(n->b, L);
    }
    case N_OR: {
        Value a = eval(n->a, L);
        if (truth(a))
            return a;
        decref(a);
        return eval(n->b, L);
    }
    case N_COND: {
        Value c = eval(n->a, L);
        int t = truth(c);
        decref(c);
        return eval(t ? n->b : n->c, L);
    }
    case N_ASSIGN: {
        Value r = eval(n->b, L);
        err_line = n->line;
        err_file = n->file;
        return assign(n->a, n->op, r, L);
    }
    case N_INCDEC: {
        Value old = lvalue_get(n->a, L);
        err_line = n->line;
        err_file = n->file;
        if (old.t != T_INT && old.t != T_FLT)
            die("%s of %s", n->op == O_ADD ? "++" : "--", type_name(old.t));
        Value nv = arith(n->op, old, vint(1));
        Value r = assign(n->a, O_SET, nv, L);
        if (n->post) {
            decref(r);
            return old;
        }
        decref(old);
        return r;
    }
    default:
        die("internal error: a statement as an expression");
    }
    return NIL;
}

static int exec(Node *n, Value *L)
{
    err_line = n->line;
    err_file = n->file;
    switch (n->k) {
    case N_BLOCK:
        for (int i = 0; i < n->nkids; i++) {
            int r = exec(n->kids[i], L);
            if (r != EX_NORMAL)
                return r;
        }
        return EX_NORMAL;
    case N_EXPR:
        decref(eval(n->a, L));
        return EX_NORMAL;
    case N_LVAR: {
        Value v = n->a ? eval(n->a, L) : NIL;
        decref(L[n->slot]);
        L[n->slot] = v;
        return EX_NORMAL;
    }
    case N_GVAR: {
        Value v = n->a ? eval(n->a, L) : NIL;
        Global *g = &globals[n->slot];
        decref(g->v);
        g->v = v;
        g->defined = 1;
        return EX_NORMAL;
    }
    case N_IF: {
        Value c = eval(n->a, L);
        int t = truth(c);
        decref(c);
        if (t)
            return exec(n->b, L);
        if (n->c)
            return exec(n->c, L);
        return EX_NORMAL;
    }
    case N_WHILE:
        for (;;) {
            Value c = eval(n->a, L);
            int t = truth(c);
            decref(c);
            if (!t)
                return EX_NORMAL;
            int r = exec(n->b, L);
            if (r == EX_BREAK)
                return EX_NORMAL;
            if (r == EX_RETURN)
                return r;
        }
    case N_DO:
        for (;;) {
            int r = exec(n->b, L);
            if (r == EX_BREAK)
                return EX_NORMAL;
            if (r == EX_RETURN)
                return r;
            Value c = eval(n->a, L);
            int t = truth(c);
            decref(c);
            if (!t)
                return EX_NORMAL;
        }
    case N_FOR:
        if (n->a) {
            int r = exec(n->a, L);
            if (r != EX_NORMAL)
                return r;
        }
        for (;;) {
            if (n->b) {
                Value c = eval(n->b, L);
                int t = truth(c);
                decref(c);
                if (!t)
                    return EX_NORMAL;
            }
            int r = exec(n->d, L);
            if (r == EX_BREAK)
                return EX_NORMAL;
            if (r == EX_RETURN)
                return r;
            if (n->c)
                exec(n->c, L);
        }
    case N_FOREACH: {
        Value c = eval(n->b, L);
        Value items;
        if (c.t == T_LIST)
            items = incref(c);
        else if (c.t == T_MAP) {
            List *l = list_new(c.u.m->n);
            for (size_t i = 0; i < c.u.m->n; i++)
                list_push(l, incref(c.u.m->k[i]));
            items = vlist(l);
        } else if (c.t == T_STR) {
            List *l = list_new(c.u.s->len);
            for (size_t i = 0; i < c.u.s->len; i++)
                list_push(l, vint((unsigned char)c.u.s->p[i]));
            items = vlist(l);
        } else
            die("for (var x : ...) of %s", type_name(c.t));
        decref(c);
        int result = EX_NORMAL;
        for (size_t i = 0; i < items.u.l->n; i++) {
            Value v = incref(items.u.l->v[i]);
            if (n->a->k == N_LOCAL) {
                decref(L[n->a->slot]);
                L[n->a->slot] = v;
            } else {
                Global *g = &globals[n->a->slot];
                decref(g->v);
                g->v = v;
                g->defined = 1;
            }
            int r = exec(n->c, L);
            if (r == EX_BREAK)
                break;
            if (r == EX_RETURN) {
                result = r;
                break;
            }
        }
        decref(items);
        return result;
    }
    case N_RETURN:
        retval = n->a ? eval(n->a, L) : NIL;
        return EX_RETURN;
    case N_BREAK:
        return EX_BREAK;
    case N_CONTINUE:
        return EX_CONTINUE;
    case N_NOP:
        return EX_NORMAL;
    default:
        decref(eval(n, L));
        return EX_NORMAL;
    }
}

static Value call_value(Value f, Value *args, int nargs)
{
    if (f.t == T_BUILTIN) {
        const Builtin *b = f.u.b;
        if (nargs < b->min || (b->max >= 0 && nargs > b->max)) {
            if (b->min == b->max)
                die("%s takes %d argument%s, not %d", b->name, b->min, b->min == 1 ? "" : "s", nargs);
            die("%s takes %d to %d arguments, not %d", b->name, b->min, b->max, nargs);
        }
        return b->fn(args, nargs);
    }
    if (f.t != T_FUNC)
        die("calling %s, which is not a function", type_name(f.t));
    Func *fn = f.u.fn;
    if (nargs > fn->nparams)
        die("%s takes %d argument%s, not %d", fn->name, fn->nparams, fn->nparams == 1 ? "" : "s", nargs);
    if (depth >= MAXDEPTH)
        die("too deep a recursion (%d calls)", depth);
    calls[depth].name = fn->name;
    calls[depth].file = err_file;
    calls[depth].line = err_line;
    depth++;
    Value localsbuf[16];
    int nl = fn->nlocals;
    Value *L = nl <= 16 ? localsbuf : xmalloc((size_t)nl * sizeof *L);
    for (int i = 0; i < nl; i++)
        L[i] = i < nargs ? incref(args[i]) : NIL;
    int r = exec(fn->body, L);
    Value result = NIL;
    if (r == EX_RETURN) {
        result = retval;
        retval = NIL;
    }
    for (int i = 0; i < nl; i++)
        decref(L[i]);
    if (L != localsbuf)
        free(L);
    depth--;
    err_file = calls[depth].file;
    err_line = calls[depth].line;
    return result;
}

/* ---- the library ---- */

#define A(i) (a[i])
#define OPT(i) (n > (i) && a[i].t != T_NIL)

static Value vbool(int b)
{
    return vint(b ? 1 : 0);
}

static int last_errno;

static Value fail_errno(void)
{
    last_errno = errno;
    return NIL;
}

/* -- output -- */

static Str *outbuf;

static void out_write(int fd, const char *p, size_t n)
{
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EPIPE)
                exit(1);
            return;
        }
        p += w;
        n -= (size_t)w;
    }
}

static void flush_all(void)
{
    if (outbuf && outbuf->len) {
        out_write(1, outbuf->p, outbuf->len);
        outbuf->len = 0;
    }
}

static void out_append(const char *p, size_t n)
{
    if (!outbuf)
        outbuf = buf_new();
    str_append(outbuf, p, n);
    if (outbuf->len >= 65536)
        flush_all();
}

static Value b_print(Value *a, int n)
{
    Str *b = buf_new();
    for (int i = 0; i < n; i++) {
        if (i)
            str_putc(b, ' ');
        repr_into(b, a[i], 0);
    }
    str_putc(b, '\n');
    out_append(b->p, b->len);
    decref(vstr_take(b));
    return NIL;
}

static Value b_printf(Value *a, int n)
{
    Str *f = need_str(a[0], "printf");
    Str *b = buf_new();
    format_into(b, f->p, f->len, a + 1, n - 1);
    out_append(b->p, b->len);
    decref(vstr_take(b));
    return NIL;
}

static Value b_eprintf(Value *a, int n)
{
    Str *f = need_str(a[0], "eprintf");
    Str *b = buf_new();
    format_into(b, f->p, f->len, a + 1, n - 1);
    flush_all();
    out_write(2, b->p, b->len);
    decref(vstr_take(b));
    return NIL;
}

static Value b_sprintf(Value *a, int n)
{
    Str *f = need_str(a[0], "sprintf");
    Str *b = buf_new();
    format_into(b, f->p, f->len, a + 1, n - 1);
    return vstr_take(b);
}

static Value b_puts(Value *a, int n)
{
    (void)n;
    Value s = to_str(a[0]);
    out_append(s.u.s->p, s.u.s->len);
    decref(s);
    return NIL;
}

static Value b_eputs(Value *a, int n)
{
    (void)n;
    Value s = to_str(a[0]);
    flush_all();
    out_write(2, s.u.s->p, s.u.s->len);
    decref(s);
    return NIL;
}

static Value b_flush(Value *a, int n)
{
    (void)a;
    (void)n;
    flush_all();
    return NIL;
}

static Value b_exit(Value *a, int n)
{
    flush_all();
    exit(n ? (int)need_int(a[0], "exit") : 0);
}

static Value b_error(Value *a, int n)
{
    Str *f = need_str(a[0], "error");
    Str *b = buf_new();
    format_into(b, f->p, f->len, a + 1, n - 1);
    die("%s", b->p);
    return NIL;
}

static Value b_assert(Value *a, int n)
{
    if (!truth(a[0])) {
        if (n > 1) {
            Value s = to_str(a[1]);
            die("assertion failed: %s", s.u.s->p);
        }
        die("assertion failed");
    }
    return NIL;
}

/* -- values -- */

static Value b_str(Value *a, int n)
{
    (void)n;
    return to_str(a[0]);
}

static Value b_repr(Value *a, int n)
{
    (void)n;
    Str *b = buf_new();
    repr_into(b, a[0], 1);
    return vstr_take(b);
}

static Value b_typeof(Value *a, int n)
{
    (void)n;
    return vstr(type_name(a[0].t));
}

/* nameof(f): the name of a function */
static Value b_nameof(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_FUNC)
        return vstr(a[0].u.fn->name);
    if (a[0].t == T_BUILTIN)
        return vstr(a[0].u.b->name);
    die("nameof: a function expected, not %s", type_name(a[0].t));
    return NIL;
}

static Value b_len(Value *a, int n)
{
    (void)n;
    switch (a[0].t) {
    case T_STR: return vint((int64_t)a[0].u.s->len);
    case T_LIST: return vint((int64_t)a[0].u.l->n);
    case T_MAP: return vint((int64_t)a[0].u.m->n);
    case T_NIL: return vint(0);
    default: die("len of %s", type_name(a[0].t));
    }
    return NIL;
}

/* int(x[, base]): a number from a string (nil if it is not one), or a
   double cut to an integer */
static Value b_int(Value *a, int n)
{
    if (a[0].t == T_INT)
        return a[0];
    if (a[0].t == T_FLT)
        return vint((int64_t)a[0].u.f);
    Str *s = need_str(a[0], "int");
    int base = n > 1 ? (int)need_int(a[1], "int") : 10;
    const char *p = s->p;
    while (isspace((unsigned char)*p))
        p++;
    if (!*p)
        return NIL;
    char *end;
    errno = 0;
    long long v = strtoll(p, &end, base);
    while (isspace((unsigned char)*end))
        end++;
    if (*end || errno)
        return NIL;
    return vint(v);
}

static Value b_num(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_INT || a[0].t == T_FLT)
        return a[0];
    Str *s = need_str(a[0], "num");
    const char *p = s->p;
    while (isspace((unsigned char)*p))
        p++;
    if (!*p)
        return NIL;
    char *end;
    errno = 0;
    long long v = strtoll(p, &end, 10);
    const char *e2 = end;
    while (isspace((unsigned char)*e2))
        e2++;
    if (!*e2 && !errno)
        return vint(v);
    double d = strtod(p, &end);
    while (isspace((unsigned char)*end))
        end++;
    if (*end)
        return NIL;
    return vflt(d);
}

static Value b_double(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_STR) {
        char *end;
        double d = strtod(a[0].u.s->p, &end);
        return *end ? NIL : vflt(d);
    }
    return vflt(need_num(a[0], "double"));
}

/* -- strings -- */

static Value b_substr(Value *a, int n)
{
    Str *s = need_str(a[0], "substr");
    int64_t st = need_int(a[1], "substr"), len = OPT(2) ? need_int(a[2], "substr") : (int64_t)s->len;
    if (st < 0)
        st = 0;
    if ((size_t)st > s->len)
        st = (int64_t)s->len;
    if (len < 0)
        len = 0;
    if ((uint64_t)st + (uint64_t)len > s->len)
        len = (int64_t)s->len - st;
    return vstrn(s->p + st, (size_t)len);
}

/* slice(x, i[, j]): a part of a string or list, j not included; negative
   positions count from the end */
static Value b_slice(Value *a, int n)
{
    int64_t len;
    if (a[0].t == T_STR)
        len = (int64_t)a[0].u.s->len;
    else
        len = (int64_t)need_list(a[0], "slice")->n;
    int64_t i = need_int(a[1], "slice"), j = OPT(2) ? need_int(a[2], "slice") : len;
    if (i < 0)
        i += len;
    if (j < 0)
        j += len;
    if (i < 0)
        i = 0;
    if (j > len)
        j = len;
    if (j < i)
        j = i;
    if (a[0].t == T_STR)
        return vstrn(a[0].u.s->p + i, (size_t)(j - i));
    List *l = list_new((size_t)(j - i));
    for (int64_t k = i; k < j; k++)
        list_push(l, incref(a[0].u.l->v[k]));
    return vlist(l);
}

static long find_sub(const char *p, size_t n, const char *q, size_t m, size_t from)
{
    if (m == 0)
        return from <= n ? (long)from : -1;
    for (size_t i = from; i + m <= n; i++)
        if (memcmp(p + i, q, m) == 0)
            return (long)i;
    return -1;
}

static Value b_index(Value *a, int n)
{
    Str *s = need_str(a[0], "index"), *t = need_str(a[1], "index");
    int64_t from = OPT(2) ? need_int(a[2], "index") : 0;
    if (from < 0)
        from = 0;
    return vint(find_sub(s->p, s->len, t->p, t->len, (size_t)from));
}

static Value b_rindex(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "rindex"), *t = need_str(a[1], "rindex");
    if (t->len > s->len)
        return vint(-1);
    for (size_t i = s->len - t->len + 1; i-- > 0;)
        if (memcmp(s->p + i, t->p, t->len) == 0)
            return vint((int64_t)i);
    return vint(-1);
}

static Value b_startswith(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "startswith"), *t = need_str(a[1], "startswith");
    return vbool(t->len <= s->len && memcmp(s->p, t->p, t->len) == 0);
}

static Value b_endswith(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "endswith"), *t = need_str(a[1], "endswith");
    return vbool(t->len <= s->len && memcmp(s->p + s->len - t->len, t->p, t->len) == 0);
}

/* split(s): at runs of blanks (none empty); split(s, sep[, max]): at each sep */
static Value b_split(Value *a, int n)
{
    Str *s = need_str(a[0], "split");
    List *l = list_new(8);
    if (!OPT(1)) {
        size_t i = 0;
        for (;;) {
            while (i < s->len && isspace((unsigned char)s->p[i]))
                i++;
            if (i >= s->len)
                break;
            size_t st = i;
            while (i < s->len && !isspace((unsigned char)s->p[i]))
                i++;
            list_push(l, vstrn(s->p + st, i - st));
        }
        return vlist(l);
    }
    Str *sep = need_str(a[1], "split");
    if (!sep->len)
        die("split with an empty separator");
    int64_t max = OPT(2) ? need_int(a[2], "split") : -1;
    size_t st = 0;
    for (;;) {
        long f = max == 0 ? -1 : find_sub(s->p, s->len, sep->p, sep->len, st);
        if (f < 0) {
            list_push(l, vstrn(s->p + st, s->len - st));
            break;
        }
        list_push(l, vstrn(s->p + st, (size_t)f - st));
        st = (size_t)f + sep->len;
        if (max > 0)
            max--;
    }
    return vlist(l);
}

/* lines(s): the lines (the last newline does not start an empty one) */
static Value b_lines(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "lines");
    List *l = list_new(8);
    size_t st = 0;
    for (size_t i = 0; i < s->len; i++)
        if (s->p[i] == '\n') {
            list_push(l, vstrn(s->p + st, i - st));
            st = i + 1;
        }
    if (st < s->len)
        list_push(l, vstrn(s->p + st, s->len - st));
    return vlist(l);
}

static Value b_join(Value *a, int n)
{
    List *l = need_list(a[0], "join");
    Str *sep = OPT(1) ? need_str(a[1], "join") : NULL;
    Str *b = buf_new();
    for (size_t i = 0; i < l->n; i++) {
        if (i && sep)
            str_append(b, sep->p, sep->len);
        repr_into(b, l->v[i], 0);
    }
    return vstr_take(b);
}

static Value b_replace(Value *a, int n)
{
    Str *s = need_str(a[0], "replace"), *o = need_str(a[1], "replace"), *w = need_str(a[2], "replace");
    int64_t count = OPT(3) ? need_int(a[3], "replace") : -1;
    if (!o->len)
        die("replace of an empty string");
    Str *b = buf_new();
    size_t st = 0;
    for (;;) {
        long f = count == 0 ? -1 : find_sub(s->p, s->len, o->p, o->len, st);
        if (f < 0)
            break;
        str_append(b, s->p + st, (size_t)f - st);
        str_append(b, w->p, w->len);
        st = (size_t)f + o->len;
        if (count > 0)
            count--;
    }
    str_append(b, s->p + st, s->len - st);
    return vstr_take(b);
}

/* the characters of a trim set: any bytes, NUL too */
static int in_set(const Str *set, char c)
{
    return memchr(set->p, c, set->len) != NULL;
}

static Value trim_common(Value *a, int n, int left, int right)
{
    static Str blanks = { 1, 6, 0, (char *)" \t\n\r\f\v" };
    Str *s = need_str(a[0], "trim");
    const Str *set = OPT(1) ? need_str(a[1], "trim") : &blanks;
    size_t i = 0, j = s->len;
    if (left)
        while (i < j && in_set(set, s->p[i]))
            i++;
    if (right)
        while (j > i && in_set(set, s->p[j - 1]))
            j--;
    return vstrn(s->p + i, j - i);
}

static Value b_trim(Value *a, int n) { return trim_common(a, n, 1, 1); }
static Value b_ltrim(Value *a, int n) { return trim_common(a, n, 1, 0); }
static Value b_rtrim(Value *a, int n) { return trim_common(a, n, 0, 1); }

static Value case_common(Value *a, int up)
{
    Str *s = need_str(a[0], up ? "upper" : "lower");
    Value r = vstrn(s->p, s->len);
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)r.u.s->p[i];
        if (c < 0x80)
            r.u.s->p[i] = (char)(up ? toupper(c) : tolower(c));
    }
    return r;
}

static Value b_upper(Value *a, int n) { (void)n; return case_common(a, 1); }
static Value b_lower(Value *a, int n) { (void)n; return case_common(a, 0); }

static Value b_repeat(Value *a, int n)
{
    (void)n;
    return arith(O_MUL, a[0], a[1]);
}

static Value b_chr(Value *a, int n)
{
    (void)n;
    char c = (char)(need_int(a[0], "chr") & 0xff);
    return vstrn(&c, 1);
}

static Value b_utf8(Value *a, int n)
{
    (void)n;
    Str *b = buf_new();
    put_utf8(b, (unsigned long)need_int(a[0], "utf8"));
    return vstr_take(b);
}

static Value b_ord(Value *a, int n)
{
    Str *s = need_str(a[0], "ord");
    int64_t i = n > 1 ? need_int(a[1], "ord") : 0;
    if (i < 0 || (size_t)i >= s->len)
        die("ord: position %lld out of range", (long long)i);
    return vint((unsigned char)s->p[i]);
}

static Value b_hex(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "hex");
    Str *b = buf_new();
    for (size_t i = 0; i < s->len; i++)
        buf_printf(b, "%02x", (unsigned char)s->p[i]);
    return vstr_take(b);
}

static Value b_unhex(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "unhex");
    Str *b = buf_new();
    int hi = -1;
    for (size_t i = 0; i < s->len; i++) {
        int c = (unsigned char)s->p[i];
        if (isspace(c))
            continue;
        if (!isxdigit(c))
            die("unhex: not a hexadecimal digit: %c", c);
        int v = isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
        if (hi < 0)
            hi = v;
        else {
            str_putc(b, hi * 16 + v);
            hi = -1;
        }
    }
    return vstr_take(b);
}

/* -- JSON, base64, SHA-256 -- */

/* the next code point of UTF-8 at *i; an invalid byte is U+FFFD */
static unsigned long utf8_next(const unsigned char *p, size_t n, size_t *i)
{
    unsigned c = p[*i];
    int len = c < 0x80 ? 1 : c >= 0xc2 && c < 0xe0 ? 2 : c >= 0xe0 && c < 0xf0 ? 3 : c >= 0xf0 && c < 0xf5 ? 4 : 0;
    if (!len || *i + (size_t)len > n) {
        (*i)++;
        return 0xfffd;
    }
    unsigned long v = len == 1 ? c : c & (0x3f >> (len - 1));
    for (int k = 1; k < len; k++) {
        if ((p[*i + (size_t)k] & 0xc0) != 0x80) {
            (*i)++;
            return 0xfffd;
        }
        v = (v << 6) | (p[*i + (size_t)k] & 0x3f);
    }
    if ((len == 3 && (v < 0x800 || (v >= 0xd800 && v < 0xe000))) || (len == 4 && (v < 0x10000 || v > 0x10ffff))) {
        (*i)++;
        return 0xfffd;
    }
    *i += (size_t)len;
    return v;
}

static void json_str(Str *b, const char *p, size_t n)
{
    str_putc(b, '"');
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)p[i];
        if (c < 0x80) {
            i++;
            switch (c) {
            case '"': str_append(b, "\\\"", 2); break;
            case '\\': str_append(b, "\\\\", 2); break;
            case '\n': str_append(b, "\\n", 2); break;
            case '\r': str_append(b, "\\r", 2); break;
            case '\t': str_append(b, "\\t", 2); break;
            case '\b': str_append(b, "\\b", 2); break;
            case '\f': str_append(b, "\\f", 2); break;
            default:
                if (c < 0x20 || c == 0x7f)
                    buf_printf(b, "\\u%04x", c);
                else
                    str_putc(b, c);
            }
            continue;
        }
        unsigned long v = utf8_next((const unsigned char *)p, n, &i);
        if (v >= 0x10000) {
            v -= 0x10000;
            buf_printf(b, "\\u%04lx\\u%04lx", 0xd800 + (v >> 10), 0xdc00 + (v & 0x3ff));
        } else
            buf_printf(b, "\\u%04lx", v);
    }
    str_putc(b, '"');
}

static int cmp_key_ptr(const void *x, const void *y)
{
    return compare(**(Value *const *)x, **(Value *const *)y);
}

static void json_into(Str *b, Value v, int indent, int level)
{
    switch (v.t) {
    case T_NIL: str_append(b, "null", 4); return;
    case T_INT: buf_printf(b, "%lld", (long long)v.u.i); return;
    case T_FLT: fmt_double(b, v.u.f); return;
    case T_STR: json_str(b, v.u.s->p, v.u.s->len); return;
    case T_LIST:
    case T_MAP: {
        int islist = v.t == T_LIST;
        size_t n = islist ? v.u.l->n : v.u.m->n;
        if (!n) {
            str_append(b, islist ? "[]" : "{}", 2);
            return;
        }
        str_putc(b, islist ? '[' : '{');
        Value **order = NULL;
        if (!islist) {
            /* the keys sorted */
            order = xmalloc(n * sizeof *order);
            for (size_t i = 0; i < n; i++)
                order[i] = &v.u.m->k[i];
            qsort(order, n, sizeof *order, cmp_key_ptr);
        }
        for (size_t i = 0; i < n; i++) {
            if (i)
                str_putc(b, ',');
            if (indent >= 0) {
                str_putc(b, '\n');
                for (int k = 0; k < indent * (level + 1); k++)
                    str_putc(b, ' ');
            } else if (i)
                str_putc(b, ' ');
            if (islist)
                json_into(b, v.u.l->v[i], indent, level + 1);
            else {
                Value k = *order[i];
                if (k.t == T_STR)
                    json_str(b, k.u.s->p, k.u.s->len);
                else
                    buf_printf(b, "\"%lld\"", (long long)k.u.i);
                str_append(b, ": ", 2);
                json_into(b, v.u.m->v[order[i] - v.u.m->k], indent, level + 1);
            }
        }
        free(order);
        if (indent >= 0) {
            str_putc(b, '\n');
            for (int k = 0; k < indent * level; k++)
                str_putc(b, ' ');
        }
        str_putc(b, islist ? ']' : '}');
        return;
    }
    default:
        die("json: %s is not data", type_name(v.t));
    }
}

/* json(v[, indent]): v in JSON (keys sorted, non-ASCII escaped as \uXXXX,
   separators ", " and ": "; with an indent, one item per line) */
static Value b_json(Value *a, int n)
{
    Str *b = buf_new();
    json_into(b, a[0], OPT(1) ? (int)need_int(a[1], "json") : -1, 0);
    return vstr_take(b);
}

typedef struct JP {
    const char *p, *e;
} JP;

static void jp_ws(JP *j)
{
    while (j->p < j->e && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r'))
        j->p++;
}

static int jp_hex4(JP *j, unsigned long *v)
{
    if (j->e - j->p < 4)
        return 0;
    *v = 0;
    for (int k = 0; k < 4; k++) {
        int c = (unsigned char)j->p[k];
        if (!isxdigit(c))
            return 0;
        *v = *v * 16 + (unsigned long)(isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
    }
    j->p += 4;
    return 1;
}

static int jp_value(JP *j, Value *out)
{
    jp_ws(j);
    if (j->p >= j->e)
        return 0;
    char c = *j->p;
    if (c == '"') {
        Str *b = buf_new();
        j->p++;
        while (j->p < j->e && *j->p != '"') {
            if (*j->p != '\\') {
                str_putc(b, *j->p++);
                continue;
            }
            j->p++;
            if (j->p >= j->e)
                break;
            char e = *j->p++;
            unsigned long u;
            switch (e) {
            case 'n': str_putc(b, '\n'); break;
            case 't': str_putc(b, '\t'); break;
            case 'r': str_putc(b, '\r'); break;
            case 'b': str_putc(b, '\b'); break;
            case 'f': str_putc(b, '\f'); break;
            case 'u':
                if (!jp_hex4(j, &u))
                    goto bad;
                if (u >= 0xd800 && u < 0xdc00 && j->e - j->p >= 6 && j->p[0] == '\\' && j->p[1] == 'u') {
                    unsigned long lo;
                    j->p += 2;
                    if (!jp_hex4(j, &lo))
                        goto bad;
                    u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
                }
                put_utf8(b, u);
                break;
            default: str_putc(b, e); break;
            }
        }
        if (j->p >= j->e) {
        bad:
            decref(vstr_take(b));
            return 0;
        }
        j->p++;
        *out = vstr_take(b);
        return 1;
    }
    if (c == '[' || c == '{') {
        int islist = c == '[';
        j->p++;
        Value v = islist ? vlist(list_new(4)) : vmap(map_new());
        jp_ws(j);
        if (j->p < j->e && *j->p == (islist ? ']' : '}')) {
            j->p++;
            *out = v;
            return 1;
        }
        for (;;) {
            Value k = NIL, x;
            if (!islist) {
                jp_ws(j);
                if (!jp_value(j, &k) || k.t != T_STR) {
                    decref(k);
                    decref(v);
                    return 0;
                }
                jp_ws(j);
                if (j->p >= j->e || *j->p != ':') {
                    decref(k);
                    decref(v);
                    return 0;
                }
                j->p++;
            }
            if (!jp_value(j, &x)) {
                decref(k);
                decref(v);
                return 0;
            }
            if (islist)
                list_push(v.u.l, x);
            else
                map_set(v.u.m, k, x);
            jp_ws(j);
            if (j->p < j->e && *j->p == ',') {
                j->p++;
                continue;
            }
            if (j->p < j->e && *j->p == (islist ? ']' : '}')) {
                j->p++;
                *out = v;
                return 1;
            }
            decref(v);
            return 0;
        }
    }
    if (j->e - j->p >= 4 && strncmp(j->p, "null", 4) == 0) {
        j->p += 4;
        *out = NIL;
        return 1;
    }
    if (j->e - j->p >= 4 && strncmp(j->p, "true", 4) == 0) {
        j->p += 4;
        *out = vint(1);
        return 1;
    }
    if (j->e - j->p >= 5 && strncmp(j->p, "false", 5) == 0) {
        j->p += 5;
        *out = vint(0);
        return 1;
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        const char *s = j->p;
        while (j->p < j->e && strchr("+-0123456789.eE", *j->p))
            j->p++;
        char tmp[64];
        size_t l = (size_t)(j->p - s) < sizeof tmp - 1 ? (size_t)(j->p - s) : sizeof tmp - 1;
        memcpy(tmp, s, l);
        tmp[l] = '\0';
        if (strpbrk(tmp, ".eE"))
            *out = vflt(strtod(tmp, NULL));
        else
            *out = vint(strtoll(tmp, NULL, 10));
        return 1;
    }
    return 0;
}

/* unjson(text): the value of JSON text (nil if it is not JSON) */
static Value b_unjson(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "unjson");
    JP j = { s->p, s->p + s->len };
    Value v;
    if (!jp_value(&j, &v))
        return NIL;
    jp_ws(&j);
    if (j.p != j.e) {
        decref(v);
        return NIL;
    }
    return v;
}

static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Value b_base64(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "base64");
    Str *b = buf_new();
    const unsigned char *p = (const unsigned char *)s->p;
    for (size_t i = 0; i < s->len; i += 3) {
        unsigned long v = (unsigned long)p[i] << 16;
        if (i + 1 < s->len)
            v |= (unsigned long)p[i + 1] << 8;
        if (i + 2 < s->len)
            v |= p[i + 2];
        str_putc(b, b64chars[(v >> 18) & 63]);
        str_putc(b, b64chars[(v >> 12) & 63]);
        str_putc(b, i + 1 < s->len ? b64chars[(v >> 6) & 63] : '=');
        str_putc(b, i + 2 < s->len ? b64chars[v & 63] : '=');
    }
    return vstr_take(b);
}

static Value b_unbase64(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "unbase64");
    Str *b = buf_new();
    unsigned long v = 0;
    int bits = 0;
    for (size_t i = 0; i < s->len; i++) {
        const char *q = strchr(b64chars, s->p[i]);
        if (!q || !s->p[i])
            continue;               /* = and white space */
        v = (v << 6) | (unsigned long)(q - b64chars);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            str_putc(b, (int)((v >> bits) & 0xff));
        }
    }
    return vstr_take(b);
}

static void sha256_block(uint32_t h[8], const unsigned char *p)
{
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t x[8];
    memcpy(x, h, sizeof x);
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(x[4], 6) ^ ROR(x[4], 11) ^ ROR(x[4], 25);
        uint32_t ch = (x[4] & x[5]) ^ (~x[4] & x[6]);
        uint32_t t1 = x[7] + S1 + ch + k[i] + w[i];
        uint32_t S0 = ROR(x[0], 2) ^ ROR(x[0], 13) ^ ROR(x[0], 22);
        uint32_t mj = (x[0] & x[1]) ^ (x[0] & x[2]) ^ (x[1] & x[2]);
        uint32_t t2 = S0 + mj;
        x[7] = x[6];
        x[6] = x[5];
        x[5] = x[4];
        x[4] = x[3] + t1;
        x[3] = x[2];
        x[2] = x[1];
        x[1] = x[0];
        x[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++)
        h[i] += x[i];
#undef ROR
}

/* sha256(s): the digest, in hexadecimal */
static Value b_sha256(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "sha256");
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    size_t i = 0;
    for (; i + 64 <= s->len; i += 64)
        sha256_block(h, (const unsigned char *)s->p + i);
    unsigned char last[128];
    size_t r = s->len - i;
    memcpy(last, s->p + i, r);
    last[r] = 0x80;
    size_t total = r + 1 + 8 <= 64 ? 64 : 128;
    memset(last + r + 1, 0, total - r - 1);
    uint64_t bits = (uint64_t)s->len * 8;
    for (int k = 0; k < 8; k++)
        last[total - 1 - (size_t)k] = (unsigned char)(bits >> (8 * k));
    sha256_block(h, last);
    if (total == 128)
        sha256_block(h, last + 64);
    Str *b = buf_new();
    for (int k = 0; k < 8; k++)
        buf_printf(b, "%08x", (unsigned)h[k]);
    return vstr_take(b);
}

/* -- regular expressions (POSIX, extended) -- */

typedef struct ReCache {
    char *src;
    int flags;
    regex_t re;
} ReCache;
static ReCache recache[64];
static int renext;

static regex_t *get_re(Str *s, const char *flagstr)
{
    int flags = REG_EXTENDED;
    if (flagstr) {
        if (strchr(flagstr, 'i'))
            flags |= REG_ICASE;
        if (strchr(flagstr, 'n'))
            flags |= REG_NEWLINE;
        if (strchr(flagstr, 'b'))
            flags &= ~REG_EXTENDED;
    }
    for (int i = 0; i < 64; i++)
        if (recache[i].src && recache[i].flags == flags && strcmp(recache[i].src, s->p) == 0)
            return &recache[i].re;
    ReCache *c = &recache[renext];
    renext = (renext + 1) % 64;
    if (c->src) {
        regfree(&c->re);
        free(c->src);
        c->src = NULL;
    }
    int r = regcomp(&c->re, s->p, flags);
    if (r) {
        char msg[256];
        regerror(r, &c->re, msg, sizeof msg);
        die("bad regular expression \"%s\": %s", s->p, msg);
    }
    c->src = xstrdup(s->p);
    c->flags = flags;
    return &c->re;
}

#define NSUB 10

/* match(s, re[, flags]): [the match, group 1, ...] or nil */
static Value b_match(Value *a, int n)
{
    Str *s = need_str(a[0], "match");
    regex_t *re = get_re(need_str(a[1], "match"), OPT(2) ? need_str(a[2], "match")->p : NULL);
    regmatch_t m[NSUB];
    if (regexec(re, s->p, NSUB, m, 0) != 0)
        return NIL;
    List *l = list_new(re->re_nsub + 1);
    for (size_t i = 0; i <= re->re_nsub && i < NSUB; i++)
        list_push(l, m[i].rm_so < 0 ? NIL : vstrn(s->p + m[i].rm_so, (size_t)(m[i].rm_eo - m[i].rm_so)));
    return vlist(l);
}

/* matchat(s, re[, flags]): [start, end] of the first match, or nil */
static Value b_matchat(Value *a, int n)
{
    Str *s = need_str(a[0], "matchat");
    regex_t *re = get_re(need_str(a[1], "matchat"), OPT(2) ? need_str(a[2], "matchat")->p : NULL);
    regmatch_t m[1];
    if (regexec(re, s->p, 1, m, 0) != 0)
        return NIL;
    List *l = list_new(2);
    list_push(l, vint(m[0].rm_so));
    list_push(l, vint(m[0].rm_eo));
    return vlist(l);
}

static Value subst(Value *a, int n, int global)
{
    Str *s = need_str(a[0], "sub"), *rp = need_str(a[2], "sub");
    regex_t *re = get_re(need_str(a[1], "sub"), OPT(3) ? need_str(a[3], "sub")->p : NULL);
    Str *b = buf_new();
    size_t pos = 0;
    int eflags = 0;
    while (pos <= s->len) {
        regmatch_t m[NSUB];
        if (regexec(re, s->p + pos, NSUB, m, eflags) != 0)
            break;
        str_append(b, s->p + pos, (size_t)m[0].rm_so);
        for (size_t i = 0; i < rp->len; i++) {
            char c = rp->p[i];
            if (c == '\\' && i + 1 < rp->len && isdigit((unsigned char)rp->p[i + 1])) {
                int g = rp->p[++i] - '0';
                if (g <= (int)re->re_nsub && m[g].rm_so >= 0)
                    str_append(b, s->p + pos + m[g].rm_so, (size_t)(m[g].rm_eo - m[g].rm_so));
            } else if (c == '\\' && i + 1 < rp->len) {
                str_putc(b, rp->p[++i]);
            } else if (c == '&') {
                str_append(b, s->p + pos + m[0].rm_so, (size_t)(m[0].rm_eo - m[0].rm_so));
            } else
                str_putc(b, c);
        }
        if (m[0].rm_eo == m[0].rm_so) {
            /* an empty match: the next byte as it is */
            if (pos + (size_t)m[0].rm_eo < s->len)
                str_putc(b, s->p[pos + (size_t)m[0].rm_eo]);
            pos += (size_t)m[0].rm_eo + 1;
        } else
            pos += (size_t)m[0].rm_eo;
        eflags = REG_NOTBOL;
        if (!global)
            break;
    }
    if (pos < s->len)
        str_append(b, s->p + pos, s->len - pos);
    return vstr_take(b);
}

static Value b_sub(Value *a, int n) { return subst(a, n, 0); }
static Value b_gsub(Value *a, int n) { return subst(a, n, 1); }

/* resplit(s, re): the parts between the matches */
static Value b_resplit(Value *a, int n)
{
    Str *s = need_str(a[0], "resplit");
    regex_t *re = get_re(need_str(a[1], "resplit"), OPT(2) ? need_str(a[2], "resplit")->p : NULL);
    List *l = list_new(8);
    size_t pos = 0;
    int eflags = 0;
    for (;;) {
        regmatch_t m[1];
        if (pos > s->len || regexec(re, s->p + pos, 1, m, eflags) != 0 || m[0].rm_eo == 0) {
            list_push(l, vstrn(s->p + pos, pos <= s->len ? s->len - pos : 0));
            break;
        }
        list_push(l, vstrn(s->p + pos, (size_t)m[0].rm_so));
        pos += (size_t)m[0].rm_eo;
        eflags = REG_NOTBOL;
    }
    return vlist(l);
}

/* -- lists and maps -- */

static Value b_list(Value *a, int n)
{
    int64_t k = need_int(a[0], "list");
    List *l = list_new(k > 0 ? (size_t)k : 1);
    for (int64_t i = 0; i < k; i++)
        list_push(l, n > 1 ? incref(a[1]) : NIL);
    return vlist(l);
}

static Value b_range(Value *a, int n)
{
    int64_t from = 0, to, step = 1;
    if (n == 1)
        to = need_int(a[0], "range");
    else {
        from = need_int(a[0], "range");
        to = need_int(a[1], "range");
        if (n > 2)
            step = need_int(a[2], "range");
    }
    if (!step)
        die("range with a step of 0");
    List *l = list_new(8);
    for (int64_t i = from; step > 0 ? i < to : i > to; i += step)
        list_push(l, vint(i));
    return vlist(l);
}

static Value b_push(Value *a, int n)
{
    List *l = need_list(a[0], "push");
    for (int i = 1; i < n; i++)
        list_push(l, incref(a[i]));
    return incref(a[0]);
}

static Value b_pop(Value *a, int n)
{
    List *l = need_list(a[0], "pop");
    if (!l->n)
        die("pop of an empty list");
    int64_t i = OPT(1) ? need_int(a[1], "pop") : (int64_t)l->n - 1;
    if (i < 0)
        i += (int64_t)l->n;
    if (i < 0 || (size_t)i >= l->n)
        die("pop: index %lld out of range", (long long)i);
    Value v = l->v[i];
    memmove(&l->v[i], &l->v[i + 1], (l->n - (size_t)i - 1) * sizeof *l->v);
    l->n--;
    return v;
}

static Value b_insert(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "insert");
    int64_t i = need_int(a[1], "insert");
    if (i < 0 || (size_t)i > l->n)
        die("insert: index %lld out of range", (long long)i);
    list_push(l, NIL);
    memmove(&l->v[i + 1], &l->v[i], (l->n - (size_t)i - 1) * sizeof *l->v);
    l->v[i] = incref(a[2]);
    return incref(a[0]);
}

static Value b_find(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "find");
    for (size_t i = 0; i < l->n; i++)
        if (values_equal(l->v[i], a[1]))
            return vint((int64_t)i);
    return vint(-1);
}

static Value sort_cmp_fn;

static int sort_cmp(const void *x, const void *y)
{
    const Value *p = x, *q = y;
    if (sort_cmp_fn.t == T_NIL)
        return compare(*p, *q);
    Value args[2] = { *p, *q };
    Value r = call_value(sort_cmp_fn, args, 2);
    int64_t c = need_int(r, "the comparison function of sort");
    decref(r);
    return c < 0 ? -1 : c > 0;
}

/* a stable sort (a merge sort: qsort is not stable) */
static void merge_sort(Value *v, size_t n, Value *tmp)
{
    if (n < 2)
        return;
    size_t h = n / 2;
    merge_sort(v, h, tmp);
    merge_sort(v + h, n - h, tmp);
    size_t i = 0, j = h, k = 0;
    while (i < h && j < n)
        tmp[k++] = sort_cmp(&v[j], &v[i]) < 0 ? v[j++] : v[i++];
    while (i < h)
        tmp[k++] = v[i++];
    while (j < n)
        tmp[k++] = v[j++];
    memcpy(v, tmp, n * sizeof *v);
}

static Value b_sort(Value *a, int n)
{
    List *l = need_list(a[0], "sort");
    Value saved = sort_cmp_fn;
    sort_cmp_fn = OPT(1) ? a[1] : NIL;
    Value *tmp = xmalloc((l->n + 1) * sizeof *tmp);
    merge_sort(l->v, l->n, tmp);
    free(tmp);
    sort_cmp_fn = saved;
    return incref(a[0]);
}

static Value b_sorted(Value *a, int n)
{
    List *src = need_list(a[0], "sorted");
    List *l = list_new(src->n);
    for (size_t i = 0; i < src->n; i++)
        list_push(l, incref(src->v[i]));
    Value lv = vlist(l);
    Value args[2] = { lv, OPT(1) ? a[1] : NIL };
    decref(b_sort(args, 2));
    return lv;
}

static Value b_reverse(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "reverse");
    for (size_t i = 0, j = l->n; i + 1 < j; i++, j--) {
        Value t = l->v[i];
        l->v[i] = l->v[j - 1];
        l->v[j - 1] = t;
    }
    return incref(a[0]);
}

static Value b_copy(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_LIST) {
        List *l = list_new(a[0].u.l->n);
        for (size_t i = 0; i < a[0].u.l->n; i++)
            list_push(l, incref(a[0].u.l->v[i]));
        return vlist(l);
    }
    if (a[0].t == T_MAP) {
        Map *m = map_new();
        for (size_t i = 0; i < a[0].u.m->n; i++)
            map_set(m, incref(a[0].u.m->k[i]), incref(a[0].u.m->v[i]));
        return vmap(m);
    }
    return incref(a[0]);
}

static Value b_keys(Value *a, int n)
{
    (void)n;
    Map *m = need_map(a[0], "keys");
    List *l = list_new(m->n);
    for (size_t i = 0; i < m->n; i++)
        list_push(l, incref(m->k[i]));
    return vlist(l);
}

static Value b_values(Value *a, int n)
{
    (void)n;
    Map *m = need_map(a[0], "values");
    List *l = list_new(m->n);
    for (size_t i = 0; i < m->n; i++)
        list_push(l, incref(m->v[i]));
    return vlist(l);
}

static Value b_haskey(Value *a, int n)
{
    (void)n;
    return vbool(map_find(need_map(a[0], "haskey"), a[1]) >= 0);
}

static Value b_del(Value *a, int n)
{
    (void)n;
    return vbool(map_del(need_map(a[0], "del"), a[1]));
}

static Value b_get(Value *a, int n)
{
    (void)n;
    Map *m = need_map(a[0], "get");
    long e = map_find(m, a[1]);
    return e >= 0 ? incref(m->v[e]) : incref(a[2]);
}

static Value b_apply(Value *a, int n)
{
    (void)n;
    List *src = need_list(a[0], "apply");
    List *l = list_new(src->n);
    for (size_t i = 0; i < src->n; i++)
        list_push(l, call_value(a[1], &src->v[i], 1));
    return vlist(l);
}

static Value b_filter(Value *a, int n)
{
    (void)n;
    List *src = need_list(a[0], "filter");
    List *l = list_new(src->n);
    for (size_t i = 0; i < src->n; i++) {
        Value r = call_value(a[1], &src->v[i], 1);
        if (truth(r))
            list_push(l, incref(src->v[i]));
        decref(r);
    }
    return vlist(l);
}

static Value minmax(Value *a, int n, int max)
{
    Value *v = a;
    int k = n;
    if (n == 1 && a[0].t == T_LIST) {
        v = a[0].u.l->v;
        k = (int)a[0].u.l->n;
    }
    if (!k)
        die("%s of nothing", max ? "max" : "min");
    Value best = v[0];
    for (int i = 1; i < k; i++)
        if ((compare(v[i], best) > 0) == max && compare(v[i], best) != 0)
            best = v[i];
    return incref(best);
}

static Value b_min(Value *a, int n) { return minmax(a, n, 0); }
static Value b_max(Value *a, int n) { return minmax(a, n, 1); }

static Value b_sum(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "sum");
    Value s = vint(0);
    for (size_t i = 0; i < l->n; i++)
        s = arith(O_ADD, s, l->v[i]);
    return s;
}

static Value b_abs(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_INT)
        return vint(a[0].u.i < 0 ? -a[0].u.i : a[0].u.i);
    return vflt(dfabs(need_num(a[0], "abs")));
}

/* -- random numbers: the same sequences everywhere -- */

static uint64_t rng_state = 0x853c49e6748fea9bULL;

static uint64_t rng_next(void)
{
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t rng_below(uint64_t k)
{
    if (k == 0)
        return 0;
    uint64_t lim = UINT64_MAX - UINT64_MAX % k;
    uint64_t x;
    do
        x = rng_next();
    while (x >= lim);
    return x % k;
}

static Value b_srand(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_STR)
        rng_state = hash_bytes(a[0].u.s->p, a[0].u.s->len);
    else
        rng_state = (uint64_t)need_int(a[0], "srand") * 0x9e3779b97f4a7c15ULL + 1;
    return NIL;
}

/* rand(): 0 to 2^31-1; rand(k): 0 to k-1; rand(i, j): i to j */
static Value b_rand(Value *a, int n)
{
    if (n == 0)
        return vint((int64_t)(rng_next() >> 33));
    if (n == 1) {
        int64_t k = need_int(a[0], "rand");
        if (k <= 0)
            die("rand(%lld)", (long long)k);
        return vint((int64_t)rng_below((uint64_t)k));
    }
    int64_t i = need_int(a[0], "rand"), j = need_int(a[1], "rand");
    if (j < i)
        die("rand(%lld, %lld)", (long long)i, (long long)j);
    return vint(i + (int64_t)rng_below((uint64_t)(j - i) + 1));
}

static Value b_frand(Value *a, int n)
{
    (void)a;
    (void)n;
    return vflt((double)(rng_next() >> 11) / 9007199254740992.0);
}

static Value b_choice(Value *a, int n)
{
    (void)n;
    if (a[0].t == T_STR) {
        if (!a[0].u.s->len)
            die("choice of an empty string");
        return vstrn(a[0].u.s->p + rng_below(a[0].u.s->len), 1);
    }
    List *l = need_list(a[0], "choice");
    if (!l->n)
        die("choice of an empty list");
    return incref(l->v[rng_below(l->n)]);
}

static Value b_shuffle(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "shuffle");
    for (size_t i = l->n; i > 1; i--) {
        size_t j = (size_t)rng_below(i);
        Value t = l->v[i - 1];
        l->v[i - 1] = l->v[j];
        l->v[j] = t;
    }
    return incref(a[0]);
}

static Value b_randbytes(Value *a, int n)
{
    (void)n;
    int64_t k = need_int(a[0], "randbytes");
    Str *b = buf_new();
    for (int64_t i = 0; i < k; i++)
        str_putc(b, (int)(rng_next() & 0xff));
    return vstr_take(b);
}

/* -- files -- */

static Value b_readfile(Value *a, int n)
{
    (void)n;
    Str *p = need_str(a[0], "readfile");
    int fd = strcmp(p->p, "-") == 0 ? 0 : open(p->p, O_RDONLY);
    if (fd < 0)
        return fail_errno();
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (fd)
            close(fd);
        errno = EISDIR;
        return fail_errno();
    }
    size_t len;
    char *buf = read_all_fd(fd, &len);
    if (fd)
        close(fd);
    Value v = vstrn(buf, len);
    free(buf);
    return v;
}

static Value write_common(Value *a, int n, int flags)
{
    Str *p = need_str(a[0], "writefile");
    Value s = to_str(a[1]);
    mode_t mode = OPT(2) ? (mode_t)need_int(a[2], "writefile") : 0666;
    int fd = strcmp(p->p, "-") == 0 ? 1 : open(p->p, O_WRONLY | O_CREAT | flags, mode);
    if (fd < 0) {
        decref(s);
        return vbool(fail_errno().t);
    }
    if (fd == 1)
        flush_all();
    size_t off = 0;
    int ok = 1;
    while (off < s.u.s->len) {
        ssize_t w = write(fd, s.u.s->p + off, s.u.s->len - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            last_errno = errno;
            ok = 0;
            break;
        }
        off += (size_t)w;
    }
    if (fd != 1 && close(fd) < 0) {
        last_errno = errno;
        ok = 0;
    }
    if (OPT(2) && fd != 1)
        chmod(p->p, mode);
    decref(s);
    return vbool(ok);
}

static Value b_writefile(Value *a, int n) { return write_common(a, n, O_TRUNC); }
static Value b_appendfile(Value *a, int n) { return write_common(a, n, O_APPEND); }

static Value stat_map(struct stat *st)
{
    Map *m = map_new();
    map_sets(m, "mode", vint(st->st_mode & 07777));
    const char *type = S_ISREG(st->st_mode) ? "f" : S_ISDIR(st->st_mode) ? "d" : S_ISLNK(st->st_mode) ? "l" :
                       S_ISFIFO(st->st_mode) ? "p" : S_ISCHR(st->st_mode) ? "c" : S_ISBLK(st->st_mode) ? "b" :
                       S_ISSOCK(st->st_mode) ? "s" : "?";
    map_sets(m, "type", vstr(type));
    map_sets(m, "uid", vint(st->st_uid));
    map_sets(m, "gid", vint(st->st_gid));
    map_sets(m, "size", vint((int64_t)st->st_size));
    map_sets(m, "nlink", vint((int64_t)st->st_nlink));
    map_sets(m, "ino", vint((int64_t)st->st_ino));
    map_sets(m, "dev", vint((int64_t)st->st_dev));
    map_sets(m, "rdev", vint((int64_t)st->st_rdev));
    map_sets(m, "atime", vint((int64_t)st->st_atime));
    map_sets(m, "mtime", vint((int64_t)st->st_mtime));
    map_sets(m, "ctime", vint((int64_t)st->st_ctime));
    /* the times in nanoseconds (st_atim, st_mtim: POSIX.1-2008) */
    map_sets(m, "atime_ns", vint((int64_t)st->st_atim.tv_sec * 1000000000 + st->st_atim.tv_nsec));
    map_sets(m, "mtime_ns", vint((int64_t)st->st_mtim.tv_sec * 1000000000 + st->st_mtim.tv_nsec));
    return vmap(m);
}

static Value b_stat(Value *a, int n)
{
    (void)n;
    struct stat st;
    if (stat(need_str(a[0], "stat")->p, &st) < 0)
        return fail_errno();
    return stat_map(&st);
}

static Value b_lstat(Value *a, int n)
{
    (void)n;
    struct stat st;
    if (lstat(need_str(a[0], "lstat")->p, &st) < 0)
        return fail_errno();
    return stat_map(&st);
}

static Value b_exists(Value *a, int n)
{
    (void)n;
    struct stat st;
    return vbool(lstat(need_str(a[0], "exists")->p, &st) == 0);
}

static Value b_isdir(Value *a, int n)
{
    (void)n;
    struct stat st;
    return vbool(stat(need_str(a[0], "isdir")->p, &st) == 0 && S_ISDIR(st.st_mode));
}

static Value b_isfile(Value *a, int n)
{
    (void)n;
    struct stat st;
    return vbool(stat(need_str(a[0], "isfile")->p, &st) == 0 && S_ISREG(st.st_mode));
}

static int rm_tree(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0)
        return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode))
        return unlink(path);
    chmod(path, 0700);
    DIR *d = opendir(path);
    if (!d)
        return -1;
    struct dirent *e;
    int r = 0;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char *sub = xmalloc(strlen(path) + strlen(e->d_name) + 2);
        sprintf(sub, "%s/%s", path, e->d_name);
        if (rm_tree(sub) < 0)
            r = -1;
        free(sub);
    }
    closedir(d);
    if (rmdir(path) < 0)
        r = -1;
    return r;
}

static Value b_remove(Value *a, int n)
{
    (void)n;
    const char *p = need_str(a[0], "remove")->p;
    struct stat st;
    if (lstat(p, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (rmdir(p) < 0)
            return vbool(fail_errno().t);
        return vbool(1);
    }
    if (unlink(p) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_rmtree(Value *a, int n)
{
    (void)n;
    if (rm_tree(need_str(a[0], "rmtree")->p) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_mkdir(Value *a, int n)
{
    if (mkdir(need_str(a[0], "mkdir")->p, OPT(1) ? (mode_t)need_int(a[1], "mkdir") : 0777) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_mkdirs(Value *a, int n)
{
    char *p = xstrdup(need_str(a[0], "mkdirs")->p);
    mode_t mode = OPT(1) ? (mode_t)need_int(a[1], "mkdirs") : 0777;
    int ok = 1;
    for (char *q = p + 1;; q++) {
        if (*q == '/' || *q == '\0') {
            char c = *q;
            *q = '\0';
            struct stat st;
            if (stat(p, &st) < 0 && mkdir(p, mode) < 0) {
                last_errno = errno;
                ok = 0;
                break;
            }
            *q = c;
            if (!c)
                break;
        }
    }
    free(p);
    return vbool(ok);
}

static int cmp_cstr(const void *x, const void *y)
{
    return strcmp(*(char *const *)x, *(char *const *)y);
}

static Value b_listdir(Value *a, int n)
{
    (void)n;
    DIR *d = opendir(need_str(a[0], "listdir")->p);
    if (!d)
        return fail_errno();
    char **names = NULL;
    size_t k = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (k == cap) {
            cap = cap ? cap * 2 : 32;
            names = xrealloc(names, cap * sizeof *names);
        }
        names[k++] = xstrdup(e->d_name);
    }
    closedir(d);
    if (k)
        qsort(names, k, sizeof *names, cmp_cstr);
    List *l = list_new(k);
    for (size_t i = 0; i < k; i++) {
        list_push(l, vstr(names[i]));
        free(names[i]);
    }
    free(names);
    return vlist(l);
}

#define PATHCALL2(name, call)                                                   \
    static Value b_##name(Value *a, int n)                                      \
    {                                                                           \
        (void)n;                                                                \
        if (call(need_str(a[0], #name)->p, need_str(a[1], #name)->p) < 0)       \
            return vbool(fail_errno().t);                                       \
        return vbool(1);                                                        \
    }
PATHCALL2(rename, rename)
PATHCALL2(symlink, symlink)
PATHCALL2(link, link)

/* abspath(path): the path made absolute with the current directory */
static Value b_abspath(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "abspath");
    if (s->len && s->p[0] == '/')
        return incref(a[0]);
    char cwd[PATH_MAX + 1];
    if (!getcwd(cwd, sizeof cwd))
        return fail_errno();
    Str *b = buf_new();
    str_append(b, cwd, strlen(cwd));
    if (!(s->len == 1 && s->p[0] == '.')) {
        const char *p = s->p;
        if (p[0] == '.' && p[1] == '/')
            p += 2;
        str_putc(b, '/');
        str_append(b, p, strlen(p));
    }
    return vstr_take(b);
}

/* dirname(path), basename(path): as the utilities */
static Value b_dirname(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "dirname");
    size_t l = s->len;
    while (l > 1 && s->p[l - 1] == '/')
        l--;
    while (l > 0 && s->p[l - 1] != '/')
        l--;
    if (l == 0)
        return vstr(".");
    while (l > 1 && s->p[l - 1] == '/')
        l--;
    return vstrn(s->p, l);
}

static Value b_basename(Value *a, int n)
{
    (void)n;
    Str *s = need_str(a[0], "basename");
    size_t e = s->len;
    while (e > 1 && s->p[e - 1] == '/')
        e--;
    size_t b = e;
    while (b > 0 && s->p[b - 1] != '/')
        b--;
    if (e == 1 && s->p[0] == '/')
        return vstr("/");
    return vstrn(s->p + b, e - b);
}

static Value b_readlink(Value *a, int n)
{
    (void)n;
    char buf[PATH_MAX + 1];
    ssize_t r = readlink(need_str(a[0], "readlink")->p, buf, PATH_MAX);
    if (r < 0)
        return fail_errno();
    return vstrn(buf, (size_t)r);
}

static Value b_chmod(Value *a, int n)
{
    (void)n;
    if (chmod(need_str(a[0], "chmod")->p, (mode_t)need_int(a[1], "chmod")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_chown(Value *a, int n)
{
    (void)n;
    if (lchown(need_str(a[0], "chown")->p, (uid_t)need_int(a[1], "chown"), (gid_t)need_int(a[2], "chown")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

/* utime(path, atime, mtime): the times, in seconds (doubles too) */
static Value b_utime(Value *a, int n)
{
    (void)n;
    struct timespec ts[2];
    for (int i = 0; i < 2; i++) {
        double t = need_num(a[i + 1], "utime");
        ts[i].tv_sec = (time_t)dfloor(t);
        ts[i].tv_nsec = (long)((t - dfloor(t)) * 1e9);
    }
    if (utimensat(AT_FDCWD, need_str(a[0], "utime")->p, ts, AT_SYMLINK_NOFOLLOW) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_mkfifo(Value *a, int n)
{
    if (mkfifo(need_str(a[0], "mkfifo")->p, OPT(1) ? (mode_t)need_int(a[1], "mkfifo") : 0666) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_access(Value *a, int n)
{
    int mode = F_OK;
    if (OPT(1)) {
        const char *m = need_str(a[1], "access")->p;
        mode = 0;
        if (strchr(m, 'r'))
            mode |= R_OK;
        if (strchr(m, 'w'))
            mode |= W_OK;
        if (strchr(m, 'x'))
            mode |= X_OK;
    }
    return vbool(access(need_str(a[0], "access")->p, mode) == 0);
}

static Value b_getcwd(Value *a, int n)
{
    (void)a;
    (void)n;
    char buf[PATH_MAX + 1];
    if (!getcwd(buf, sizeof buf))
        return fail_errno();
    return vstr(buf);
}

static Value b_chdir(Value *a, int n)
{
    (void)n;
    if (chdir(need_str(a[0], "chdir")->p) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_umask(Value *a, int n)
{
    (void)n;
    return vint(umask((mode_t)need_int(a[0], "umask")));
}

static Value b_truncate(Value *a, int n)
{
    (void)n;
    if (truncate(need_str(a[0], "truncate")->p, (off_t)need_int(a[1], "truncate")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

/* mkdtemp([prefix]): a new directory in $TMPDIR (or /tmp) */
static Value b_mkdtemp(Value *a, int n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir)
        dir = "/tmp";
    const char *pre = OPT(0) ? need_str(a[0], "mkdtemp")->p : "tl.";
    char *t = xmalloc(strlen(dir) + strlen(pre) + 10);
    sprintf(t, "%s/%sXXXXXX", dir, pre);
    if (!mkdtemp(t)) {
        free(t);
        return fail_errno();
    }
    Value v = vstr(t);
    free(t);
    return v;
}

static Value b_errno(Value *a, int n)
{
    (void)a;
    (void)n;
    return vint(last_errno);
}

static Value b_strerror(Value *a, int n)
{
    return vstr(strerror(n ? (int)need_int(a[0], "strerror") : last_errno));
}

/* -- locales -- */

/* setlocale(name[, category]): the locale of the C library ("": that of
   the environment); category: "all" (the default), "ctype", "collate",
   "messages", "monetary", "numeric", "time"; the name, or nil */
static Value b_setlocale(Value *a, int n)
{
    int cat = LC_ALL;
    if (OPT(1)) {
        const char *c = need_str(a[1], "setlocale")->p;
        cat = strcmp(c, "ctype") == 0 ? LC_CTYPE : strcmp(c, "collate") == 0 ? LC_COLLATE :
              strcmp(c, "messages") == 0 ? LC_MESSAGES : strcmp(c, "monetary") == 0 ? LC_MONETARY :
              strcmp(c, "numeric") == 0 ? LC_NUMERIC : strcmp(c, "time") == 0 ? LC_TIME : LC_ALL;
    }
    const char *r = setlocale(cat, need_str(a[0], "setlocale")->p);
    return r ? vstr(r) : NIL;
}

/* iswctype(wc, class): is the character in the class of the locale (nil:
   the locale has no such class) */
static Value b_iswctype(Value *a, int n)
{
    (void)n;
    wctype_t t = wctype(need_str(a[1], "iswctype")->p);
    if (!t)
        return NIL;
    return vbool(iswctype((wint_t)need_int(a[0], "iswctype"), t));
}

/* -- the environment -- */

static Value b_getenv(Value *a, int n)
{
    (void)n;
    const char *v = getenv(need_str(a[0], "getenv")->p);
    return v ? vstr(v) : NIL;
}

static Value b_setenv(Value *a, int n)
{
    (void)n;
    if (a[1].t == T_NIL)
        unsetenv(need_str(a[0], "setenv")->p);
    else {
        Value v = to_str(a[1]);
        setenv(need_str(a[0], "setenv")->p, v.u.s->p, 1);
        decref(v);
    }
    return NIL;
}

static Value b_unsetenv(Value *a, int n)
{
    (void)n;
    unsetenv(need_str(a[0], "unsetenv")->p);
    return NIL;
}

static Value b_environ(Value *a, int n)
{
    (void)a;
    (void)n;
    Map *m = map_new();
    for (char **e = environ; *e; e++) {
        const char *eq = strchr(*e, '=');
        if (eq)
            map_set(m, vstrn(*e, (size_t)(eq - *e)), vstr(eq + 1));
    }
    return vmap(m);
}

/* -- processes -- */

static int64_t status_of(int st)
{
    if (WIFEXITED(st))
        return WEXITSTATUS(st);
    if (WIFSIGNALED(st))
        return 128 + WTERMSIG(st);
    return -1;
}

static char **argv_of(Value cmd, const char *what)
{
    char **argv;
    if (cmd.t == T_STR) {
        argv = xmalloc(4 * sizeof *argv);
        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = cmd.u.s->p;
        argv[3] = NULL;
        return argv;
    }
    List *l = need_list(cmd, what);
    if (!l->n)
        die("%s: an empty command", what);
    argv = xmalloc((l->n + 1) * sizeof *argv);
    for (size_t i = 0; i < l->n; i++) {
        Value s = to_str(l->v[i]);
        argv[i] = xstrdup(s.u.s->p);
        decref(s);
    }
    argv[l->n] = NULL;
    return argv;
}

/* in the child: the environment, directory and descriptors of opts */
static void child_setup(Map *opts)
{
    if (!opts)
        return;
    Value env = map_gets(opts, "env");
    if (env.t == T_MAP)
        for (size_t i = 0; i < env.u.m->n; i++) {
            Value k = env.u.m->k[i], v = env.u.m->v[i];
            if (k.t != T_STR)
                continue;
            if (v.t == T_NIL)
                unsetenv(k.u.s->p);
            else {
                Value s = to_str(v);
                setenv(k.u.s->p, s.u.s->p, 1);
            }
        }
    Value cwd = map_gets(opts, "cwd");
    if (cwd.t == T_STR && chdir(cwd.u.s->p) < 0) {
        fprintf(stderr, "tl: %s: %s\n", cwd.u.s->p, strerror(errno));
        _exit(127);
    }
    Value um = map_gets(opts, "umask");
    if (um.t == T_INT)
        umask((mode_t)um.u.i);
    Value sid = map_gets(opts, "setsid");
    if (truth(sid))
        setsid();
    Value pg = map_gets(opts, "setpgid");
    if (truth(pg))
        setpgid(0, 0);
}

static void exec_argv(char **argv, int is_shell)
{
    if (is_shell)
        execv("/bin/sh", argv);
    else
        execvp(argv[0], argv);
    fprintf(stderr, "tl: %s: %s\n", argv[0], strerror(errno));
    _exit(errno == ENOENT ? 127 : 126);
}

/* the disposition of a standard descriptor: inherit, null, pipe, out
   (the standard output's), or a file name ">name", ">>name", "<name" */
static int open_disp(const char *d, int fd)
{
    if (strcmp(d, "null") == 0)
        return open("/dev/null", fd == 0 ? O_RDONLY : O_WRONLY);
    if (strncmp(d, ">>", 2) == 0)
        return open(d + 2, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (d[0] == '>')
        return open(d + 1, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (d[0] == '<')
        return open(d + 1, O_RDONLY);
    return -2;
}

static const char *opt_str(Map *opts, const char *k, const char *def)
{
    if (!opts)
        return def;
    Value v = map_gets(opts, k);
    return v.t == T_STR ? v.u.s->p : def;
}

static double opt_num(Map *opts, const char *k, double def)
{
    if (!opts)
        return def;
    Value v = map_gets(opts, k);
    return v.t == T_INT ? (double)v.u.i : v.t == T_FLT ? v.u.f : def;
}

static double now_secs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void set_cloexec(int fd)
{
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

/*
 * run(cmd[, opts]): run a command (a string: by sh -c; a list: the
 * program and its arguments) and wait for it; a map: out, err (the
 * outputs), status (the exit status, 128+n for signal n), timedout.
 * opts: in (a string: the standard input), out ("pipe": caught, the
 * default; "inherit", "null", ">file"...), err ("pipe" the default,
 * "out", "inherit", "null", ...), env (a map; nil values unset), cwd,
 * timeout (seconds), umask, setsid, setpgid.
 */
static Value b_run(Value *a, int n)
{
    Map *opts = OPT(1) ? need_map(a[1], "run") : NULL;
    char **argv = argv_of(a[0], "run");
    Value inv = opts ? map_gets(opts, "in") : NIL;
    const char *outd = opt_str(opts, "out", "pipe"), *errd = opt_str(opts, "err", "pipe");
    const char *ind = opt_str(opts, "stdin", "inherit");
    double timeout = opt_num(opts, "timeout", 0);
    int inp[2] = { -1, -1 }, outp[2] = { -1, -1 }, errp[2] = { -1, -1 };
    if (inv.t != T_NIL && pipe(inp) < 0)
        die("run: pipe: %s", strerror(errno));
    if (strcmp(outd, "pipe") == 0 && pipe(outp) < 0)
        die("run: pipe: %s", strerror(errno));
    if (strcmp(errd, "pipe") == 0 && pipe(errp) < 0)
        die("run: pipe: %s", strerror(errno));
    flush_all();
    pid_t pid = fork();
    if (pid < 0)
        die("run: fork: %s", strerror(errno));
    if (pid == 0) {
        if (inp[0] >= 0) {
            dup2(inp[0], 0);
            close(inp[0]);
            close(inp[1]);
        } else {
            int fd = open_disp(ind, 0);
            if (fd >= 0) {
                dup2(fd, 0);
                close(fd);
            }
        }
        if (outp[1] >= 0) {
            dup2(outp[1], 1);
            close(outp[0]);
            close(outp[1]);
        } else {
            int fd = open_disp(outd, 1);
            if (fd >= 0) {
                dup2(fd, 1);
                close(fd);
            }
        }
        if (errp[1] >= 0) {
            dup2(errp[1], 2);
            close(errp[0]);
            close(errp[1]);
        } else if (strcmp(errd, "out") == 0)
            dup2(1, 2);
        else {
            int fd = open_disp(errd, 2);
            if (fd >= 0) {
                dup2(fd, 2);
                close(fd);
            }
        }
        child_setup(opts);
        signal(SIGPIPE, SIG_DFL);
        exec_argv(argv, a[0].t == T_STR);
    }
    if (inp[0] >= 0)
        close(inp[0]);
    if (outp[1] >= 0)
        close(outp[1]);
    if (errp[1] >= 0)
        close(errp[1]);
    Str *out = buf_new(), *err = buf_new();
    size_t inoff = 0;
    Str *in = inv.t == T_NIL ? NULL : need_str(inv, "run: in");
    if (in && in->len == 0) {
        close(inp[1]);
        inp[1] = -1;
    }
    if (inp[1] >= 0)
        fcntl(inp[1], F_SETFL, fcntl(inp[1], F_GETFL) | O_NONBLOCK);
    double end = timeout > 0 ? now_secs() + timeout : 0;
    int timedout = 0;
    /* what a timeout kills: the process, or its group if it has one of its
       own (setsid or setpgid) */
    pid_t victim = pid;
    if (opts && (truth(map_gets(opts, "setsid")) || truth(map_gets(opts, "setpgid"))))
        victim = -pid;
    while (outp[0] >= 0 || errp[0] >= 0 || inp[1] >= 0) {
        struct pollfd pf[3];
        int np = 0, io = -1, oo = -1, eo = -1;
        if (inp[1] >= 0) {
            pf[np].fd = inp[1];
            pf[np].events = POLLOUT;
            io = np++;
        }
        if (outp[0] >= 0) {
            pf[np].fd = outp[0];
            pf[np].events = POLLIN;
            oo = np++;
        }
        if (errp[0] >= 0) {
            pf[np].fd = errp[0];
            pf[np].events = POLLIN;
            eo = np++;
        }
        int ms = -1;
        if (end) {
            double left = end - now_secs();
            if (left <= 0) {
                kill(victim, SIGKILL);
                timedout = 1;
                break;
            }
            ms = (int)(left * 1000) + 1;
        }
        int r = poll(pf, (nfds_t)np, ms);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (io >= 0 && pf[io].revents) {
            ssize_t w = write(inp[1], in->p + inoff, in->len - inoff);
            if (w > 0)
                inoff += (size_t)w;
            if (w < 0 && errno != EAGAIN && errno != EINTR)
                inoff = in->len;
            if (inoff >= in->len) {
                close(inp[1]);
                inp[1] = -1;
            }
        }
        char buf[65536];
        if (oo >= 0 && pf[oo].revents) {
            ssize_t k = read(outp[0], buf, sizeof buf);
            if (k > 0)
                str_append(out, buf, (size_t)k);
            else if (k == 0 || errno != EINTR) {
                close(outp[0]);
                outp[0] = -1;
            }
        }
        if (eo >= 0 && pf[eo].revents) {
            ssize_t k = read(errp[0], buf, sizeof buf);
            if (k > 0)
                str_append(err, buf, (size_t)k);
            else if (k == 0 || errno != EINTR) {
                close(errp[0]);
                errp[0] = -1;
            }
        }
    }
    if (inp[1] >= 0)
        close(inp[1]);
    if (outp[0] >= 0)
        close(outp[0]);
    if (errp[0] >= 0)
        close(errp[0]);
    int st = 0;
    if (!timedout && end) {
        /* the outputs closed: wait the rest of the time for the process */
        for (;;) {
            pid_t w = waitpid(pid, &st, WNOHANG);
            if (w == pid)
                break;
            if (now_secs() >= end) {
                kill(victim, SIGKILL);
                timedout = 1;
                waitpid(pid, &st, 0);
                break;
            }
            struct timespec ts = { 0, 10000000 };
            nanosleep(&ts, NULL);
        }
    } else
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
            ;
    if (a[0].t != T_STR) {
        for (char **p = argv; *p; p++)
            free(*p);
    }
    free(argv);
    Map *m = map_new();
    map_sets(m, "out", vstr_take(out));
    map_sets(m, "err", vstr_take(err));
    map_sets(m, "status", vint(status_of(st)));
    map_sets(m, "signal", vint(WIFSIGNALED(st) ? WTERMSIG(st) : 0));
    map_sets(m, "timedout", vbool(timedout));
    return vmap(m);
}

/* sh(cmd[, input]): the standard output of sh -c cmd */
static Value b_sh(Value *a, int n)
{
    Map *o = map_new();
    map_sets(o, "err", vstr("inherit"));
    if (OPT(1))
        map_sets(o, "in", incref(a[1]));
    Value args[2] = { a[0], vmap(o) };
    Value r = b_run(args, 2);
    decref(args[1]);
    Value out = incref(map_gets(r.u.m, "out"));
    decref(r);
    return out;
}

static Value b_system(Value *a, int n)
{
    (void)n;
    Map *o = map_new();
    map_sets(o, "out", vstr("inherit"));
    map_sets(o, "err", vstr("inherit"));
    Value args[2] = { a[0], vmap(o) };
    Value r = b_run(args, 2);
    decref(args[1]);
    Value st = map_gets(r.u.m, "status");
    decref(r);
    return st;
}

/* spawn(cmd[, opts]): start a command, not waiting: its process ID.
   opts: in, out, err ("inherit", the default; "null", ">file" ..., or a
   descriptor), and those of run */
static Value b_spawn(Value *a, int n)
{
    Map *opts = OPT(1) ? need_map(a[1], "spawn") : NULL;
    char **argv = argv_of(a[0], "spawn");
    flush_all();
    pid_t pid = fork();
    if (pid < 0)
        die("spawn: fork: %s", strerror(errno));
    if (pid == 0) {
        static const char *const names[] = { "in", "out", "err" };
        for (int fd = 0; fd < 3; fd++) {
            Value dv = opts ? map_gets(opts, names[fd]) : NIL;
            if (dv.t == T_INT) {            /* a descriptor */
                if (dv.u.i != fd)
                    dup2((int)dv.u.i, fd);
                else
                    fcntl(fd, F_SETFD, 0);
                continue;
            }
            const char *d = opt_str(opts, names[fd], "inherit");
            if (fd == 2 && strcmp(d, "out") == 0) {
                dup2(1, 2);
                continue;
            }
            int f = open_disp(d, fd);
            if (f >= 0) {
                dup2(f, fd);
                close(f);
            }
        }
        child_setup(opts);
        signal(SIGPIPE, SIG_DFL);
        exec_argv(argv, a[0].t == T_STR);
    }
    if (a[0].t != T_STR)
        for (char **p = argv; *p; p++)
            free(*p);
    free(argv);
    return vint(pid);
}

static int last_termsig;

/* waitpid(pid[, nohang]): the exit status; nil if nohang and it has not
   ended; -1 for an error */
static Value b_waitpid(Value *a, int n)
{
    int st;
    pid_t r;
    while ((r = waitpid((pid_t)need_int(a[0], "waitpid"), &st, OPT(1) && truth(a[1]) ? WNOHANG : 0)) < 0 &&
           errno == EINTR)
        ;
    if (r == 0)
        return NIL;
    if (r < 0) {
        last_errno = errno;
        return vint(-1);
    }
    last_termsig = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
    return vint(status_of(st));
}

/* termsig(): the signal that ended the process waitpid() last waited for,
   0 if it exited */
static Value b_termsig(Value *a, int n)
{
    (void)a;
    (void)n;
    return vint(last_termsig);
}

static Value b_fork(Value *a, int n)
{
    (void)a;
    (void)n;
    flush_all();
    pid_t p = fork();
    if (p < 0)
        return vint(fail_errno().t - 1);
    return vint(p);
}

/* exec(argv): replaces the program; returns only if it cannot (false) */
static Value b_exec(Value *a, int n)
{
    (void)n;
    char **argv = argv_of(a[0], "exec");
    flush_all();
    signal(SIGPIPE, SIG_DFL);
    if (a[0].t == T_STR)
        execv("/bin/sh", argv);
    else
        execvp(argv[0], argv);
    last_errno = errno;
    return vbool(0);
}

static Value b__exit(Value *a, int n)
{
    (void)n;
    _exit((int)need_int(a[0], "_exit"));
}

#define INTCALL0(name, expr)                    \
    static Value b_##name(Value *a, int n)      \
    {                                           \
        (void)a;                                \
        (void)n;                                \
        return vint((int64_t)(expr));           \
    }
INTCALL0(getpid, getpid())
INTCALL0(getppid, getppid())
INTCALL0(getpgrp, getpgrp())
INTCALL0(getuid, getuid())
INTCALL0(geteuid, geteuid())
INTCALL0(getgid, getgid())
INTCALL0(getegid, getegid())
INTCALL0(time, time(NULL))

/* nice(incr): the new nice value */
static Value b_nice(Value *a, int n)
{
    int r;
    (void)n;
    errno = 0;
    r = nice((int)need_int(a[0], "nice"));
    if (r == -1 && errno != 0)
        return vint(-100);
    return vint(r);
}

/* times(): the processor times of the process and of its children waited
   for, in seconds: {user, sys, cuser, csys} */
static Value b_times(Value *a, int n)
{
    struct tms t;
    double hz = (double)sysconf(_SC_CLK_TCK);
    Map *m = map_new();
    (void)a;
    (void)n;
    times(&t);
    map_sets(m, "user", vflt((double)t.tms_utime / hz));
    map_sets(m, "sys", vflt((double)t.tms_stime / hz));
    map_sets(m, "cuser", vflt((double)t.tms_cutime / hz));
    map_sets(m, "csys", vflt((double)t.tms_cstime / hz));
    return vmap(m);
}

static Value b_now(Value *a, int n)
{
    (void)a;
    (void)n;
    return vflt(now_secs());
}

static Value b_getsid(Value *a, int n)
{
    pid_t r = getsid(n ? (pid_t)need_int(a[0], "getsid") : 0);
    if (r < 0)
        last_errno = errno;
    return vint(r);
}

static Value b_setsid(Value *a, int n)
{
    (void)a;
    (void)n;
    pid_t r = setsid();
    if (r < 0)
        last_errno = errno;
    return vint(r);
}

static Value b_setpgid(Value *a, int n)
{
    (void)n;
    if (setpgid((pid_t)need_int(a[0], "setpgid"), (pid_t)need_int(a[1], "setpgid")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_kill(Value *a, int n)
{
    (void)n;
    if (kill((pid_t)need_int(a[0], "kill"), (int)need_int(a[1], "kill")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

#define IDCALL1(name, type)                                             \
    static Value b_##name(Value *a, int n)                              \
    {                                                                   \
        (void)n;                                                        \
        if (name((type)need_int(a[0], #name)) < 0)                      \
            return vbool(fail_errno().t);                               \
        return vbool(1);                                                \
    }
IDCALL1(setuid, uid_t)
IDCALL1(seteuid, uid_t)
IDCALL1(setgid, gid_t)
IDCALL1(setegid, gid_t)

static Value b_setreuid(Value *a, int n)
{
    (void)n;
    if (setreuid((uid_t)need_int(a[0], "setreuid"), (uid_t)need_int(a[1], "setreuid")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_setregid(Value *a, int n)
{
    (void)n;
    if (setregid((gid_t)need_int(a[0], "setregid"), (gid_t)need_int(a[1], "setregid")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_setresuid(Value *a, int n)
{
    (void)n;
    if (setresuid((uid_t)need_int(a[0], "setresuid"), (uid_t)need_int(a[1], "setresuid"),
                  (uid_t)need_int(a[2], "setresuid")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_setresgid(Value *a, int n)
{
    (void)n;
    if (setresgid((gid_t)need_int(a[0], "setresgid"), (gid_t)need_int(a[1], "setresgid"),
                  (gid_t)need_int(a[2], "setresgid")) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_getgroups(Value *a, int n)
{
    (void)a;
    (void)n;
    int k = getgroups(0, NULL);
    gid_t *g = xmalloc((size_t)(k > 0 ? k : 1) * sizeof *g);
    k = getgroups(k, g);
    List *l = list_new((size_t)(k > 0 ? k : 1));
    for (int i = 0; i < k; i++)
        list_push(l, vint(g[i]));
    free(g);
    return vlist(l);
}

static Value b_setgroups(Value *a, int n)
{
    (void)n;
    List *l = need_list(a[0], "setgroups");
    gid_t *g = xmalloc((l->n + 1) * sizeof *g);
    for (size_t i = 0; i < l->n; i++)
        g[i] = (gid_t)need_int(l->v[i], "setgroups");
#if defined(__linux__)
    int r = setgroups(l->n, g);
#else
    int r = -1;
    errno = ENOSYS;
#endif
    free(g);
    if (r < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value pw_map(struct passwd *pw)
{
    if (!pw)
        return NIL;
    Map *m = map_new();
    map_sets(m, "name", vstr(pw->pw_name));
    map_sets(m, "uid", vint(pw->pw_uid));
    map_sets(m, "gid", vint(pw->pw_gid));
    map_sets(m, "dir", vstr(pw->pw_dir));
    map_sets(m, "shell", vstr(pw->pw_shell));
    return vmap(m);
}

static Value gr_map(struct group *gr)
{
    if (!gr)
        return NIL;
    Map *m = map_new();
    map_sets(m, "name", vstr(gr->gr_name));
    map_sets(m, "gid", vint(gr->gr_gid));
    List *l = list_new(4);
    for (char **p = gr->gr_mem; p && *p; p++)
        list_push(l, vstr(*p));
    map_sets(m, "mem", vlist(l));
    return vmap(m);
}

static Value b_getpwuid(Value *a, int n) { (void)n; return pw_map(getpwuid((uid_t)need_int(a[0], "getpwuid"))); }
static Value b_getpwnam(Value *a, int n) { (void)n; return pw_map(getpwnam(need_str(a[0], "getpwnam")->p)); }
static Value b_getgrgid(Value *a, int n) { (void)n; return gr_map(getgrgid((gid_t)need_int(a[0], "getgrgid"))); }
static Value b_getgrnam(Value *a, int n) { (void)n; return gr_map(getgrnam(need_str(a[0], "getgrnam")->p)); }

/* -- signals -- */

static const struct {
    const char *name;
    int sig;
} signames[] = {
    { "SIGHUP", SIGHUP }, { "SIGINT", SIGINT }, { "SIGQUIT", SIGQUIT }, { "SIGILL", SIGILL },
    { "SIGTRAP", SIGTRAP }, { "SIGABRT", SIGABRT }, { "SIGBUS", SIGBUS }, { "SIGFPE", SIGFPE },
    { "SIGKILL", SIGKILL }, { "SIGUSR1", SIGUSR1 }, { "SIGSEGV", SIGSEGV }, { "SIGUSR2", SIGUSR2 },
    { "SIGPIPE", SIGPIPE }, { "SIGALRM", SIGALRM }, { "SIGTERM", SIGTERM }, { "SIGCHLD", SIGCHLD },
    { "SIGCONT", SIGCONT }, { "SIGSTOP", SIGSTOP }, { "SIGTSTP", SIGTSTP }, { "SIGTTIN", SIGTTIN },
    { "SIGTTOU", SIGTTOU }, { "SIGURG", SIGURG }, { "SIGXCPU", SIGXCPU }, { "SIGXFSZ", SIGXFSZ },
    { "SIGVTALRM", SIGVTALRM }, { "SIGPROF", SIGPROF }, { "SIGSYS", SIGSYS },
#ifdef SIGWINCH
    { "SIGWINCH", SIGWINCH },
#endif
    { NULL, 0 }
};

static void caught(int sig)
{
    (void)sig;
}

/* signal(sig, how): "dfl", "ign" or "catch" (a handler that does
   nothing, so that pause() returns) */
static Value b_signal(Value *a, int n)
{
    (void)n;
    int sig = (int)need_int(a[0], "signal");
    const char *how = need_str(a[1], "signal")->p;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    if (strcmp(how, "dfl") == 0)
        sa.sa_handler = SIG_DFL;
    else if (strcmp(how, "ign") == 0)
        sa.sa_handler = SIG_IGN;
    else if (strcmp(how, "catch") == 0)
        sa.sa_handler = caught;
    else
        die("signal: dfl, ign or catch, not %s", how);
    if (sigaction(sig, &sa, NULL) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

/* sigdisp(sig): "dfl", "ign" or "caught" */
static Value b_sigdisp(Value *a, int n)
{
    (void)n;
    struct sigaction sa;
    if (sigaction((int)need_int(a[0], "sigdisp"), NULL, &sa) < 0)
        return fail_errno();
    return vstr(sa.sa_handler == SIG_DFL ? "dfl" : sa.sa_handler == SIG_IGN ? "ign" : "caught");
}

/* sigmask(sig, block): blocks (true) or unblocks a signal */
static Value b_sigmask(Value *a, int n)
{
    (void)n;
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, (int)need_int(a[0], "sigmask"));
    sigprocmask(truth(a[1]) ? SIG_BLOCK : SIG_UNBLOCK, &s, NULL);
    return NIL;
}

static Value b_pause(Value *a, int n)
{
    (void)a;
    (void)n;
    flush_all();
    pause();
    return NIL;
}

static Value b_sleep(Value *a, int n)
{
    (void)n;
    double s = need_num(a[0], "sleep");
    flush_all();
    if (s <= 0)
        return NIL;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
    return NIL;
}

/* -- descriptors -- */

/* open(path, how[, mode]): how has r, w (both: rw), a (append), c
   (create), t (truncate), x (exclusive), n (non-blocking), e (close on
   exec); a descriptor, or -1 */
static Value b_open(Value *a, int n)
{
    const char *how = need_str(a[1], "open")->p;
    int r = strchr(how, 'r') != NULL, w = strchr(how, 'w') != NULL || strchr(how, 'a') != NULL;
    int fl = r && w ? O_RDWR : w ? O_WRONLY : O_RDONLY;
    if (strchr(how, 'a'))
        fl |= O_APPEND;
    if (strchr(how, 'c'))
        fl |= O_CREAT;
    if (strchr(how, 't'))
        fl |= O_TRUNC;
    if (strchr(how, 'x'))
        fl |= O_EXCL;
    if (strchr(how, 'n'))
        fl |= O_NONBLOCK;
    if (strchr(how, 'e'))
        fl |= O_CLOEXEC;
    int fd = open(need_str(a[0], "open")->p, fl, OPT(2) ? (mode_t)need_int(a[2], "open") : 0666);
    if (fd < 0)
        last_errno = errno;
    return vint(fd);
}

static Value b_close(Value *a, int n)
{
    (void)n;
    return vbool(close((int)need_int(a[0], "close")) == 0);
}

static Value b_dup2(Value *a, int n)
{
    (void)n;
    int r = dup2((int)need_int(a[0], "dup2"), (int)need_int(a[1], "dup2"));
    if (r < 0)
        last_errno = errno;
    return vint(r);
}

/* pipe(): [read end, write end], closed on exec (a program started gets
   one as a standard descriptor of spawn()) */
static Value b_pipe(Value *a, int n)
{
    (void)a;
    (void)n;
    int p[2];
    if (pipe(p) < 0)
        return fail_errno();
    fcntl(p[0], F_SETFD, FD_CLOEXEC);
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
    List *l = list_new(2);
    list_push(l, vint(p[0]));
    list_push(l, vint(p[1]));
    return vlist(l);
}

/* fdread(fd[, max[, timeout]]): what can be read (up to max bytes), ""
   at the end, nil after timeout seconds (or an error) */
static Value b_fdread(Value *a, int n)
{
    int fd = (int)need_int(a[0], "fdread");
    size_t max = OPT(1) ? (size_t)need_int(a[1], "fdread") : 65536;
    if (OPT(2)) {
        struct pollfd pf = { fd, POLLIN, 0 };
        int r;
        while ((r = poll(&pf, 1, (int)(need_num(a[2], "fdread") * 1000))) < 0 && errno == EINTR)
            ;
        if (r <= 0)
            return NIL;
    }
    char *buf = xmalloc(max ? max : 1);
    ssize_t r;
    while ((r = read(fd, buf, max)) < 0 && errno == EINTR)
        ;
    if (r < 0) {
        free(buf);
        return fail_errno();
    }
    Value v = vstrn(buf, (size_t)r);
    free(buf);
    return v;
}

static Value b_fdwrite(Value *a, int n)
{
    (void)n;
    int fd = (int)need_int(a[0], "fdwrite");
    Str *s = need_str(a[1], "fdwrite");
    if (fd == 1)
        flush_all();
    size_t off = 0;
    while (off < s->len) {
        ssize_t w = write(fd, s->p + off, s->len - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return vbool(fail_errno().t);
        }
        off += (size_t)w;
    }
    return vbool(1);
}

static Value b_isatty(Value *a, int n)
{
    (void)n;
    return vbool(isatty((int)need_int(a[0], "isatty")));
}

static Value b_ttyname(Value *a, int n)
{
    (void)n;
    const char *t = ttyname((int)need_int(a[0], "ttyname"));
    return t ? vstr(t) : fail_errno();
}

/* mmap(path): maps the file (shared, read and write), until the end */
static Value b_mmap(Value *a, int n)
{
    (void)n;
    int fd = open(need_str(a[0], "mmap")->p, O_RDWR);
    if (fd < 0)
        return vbool(fail_errno().t);
    struct stat st;
    fstat(fd, &st);
    void *p = mmap(NULL, st.st_size ? (size_t)st.st_size : 1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED)
        return vbool(fail_errno().t);
    return vbool(1);
}

static int unix_addr(struct sockaddr_un *sa, const char *path)
{
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    if (strlen(path) >= sizeof sa->sun_path) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(sa->sun_path, path);
    return 0;
}

static Value b_unixconnect(Value *a, int n)
{
    (void)n;
    struct sockaddr_un sa;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0 || unix_addr(&sa, need_str(a[0], "unixconnect")->p) < 0 ||
        connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        last_errno = errno;
        if (fd >= 0)
            close(fd);
        return vint(-1);
    }
    return vint(fd);
}

static Value b_unixlisten(Value *a, int n)
{
    (void)n;
    struct sockaddr_un sa;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0 || unix_addr(&sa, need_str(a[0], "unixlisten")->p) < 0 ||
        bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(fd, 8) < 0) {
        last_errno = errno;
        if (fd >= 0)
            close(fd);
        return vint(-1);
    }
    return vint(fd);
}

/* unixdgram(path): a datagram socket bound to path (fdread() gets a datagram) */
static Value b_unixdgram(Value *a, int n)
{
    (void)n;
    struct sockaddr_un sa;
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0 || unix_addr(&sa, need_str(a[0], "unixdgram")->p) < 0 ||
        bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        last_errno = errno;
        if (fd >= 0)
            close(fd);
        return vint(-1);
    }
    return vint(fd);
}

/* lockf(fd, how): how "lock" (waits), "tlock" (does not wait), "ulock" */
static Value b_lockf(Value *a, int n)
{
    (void)n;
    const char *how = need_str(a[1], "lockf")->p;
    int cmd = strcmp(how, "lock") == 0 ? F_LOCK : strcmp(how, "tlock") == 0 ? F_TLOCK : strcmp(how, "ulock") == 0 ? F_ULOCK : -1;
    if (cmd < 0)
        die("lockf: lock, tlock or ulock, not %s", how);
    if (lockf((int)need_int(a[0], "lockf"), cmd, 0) < 0)
        return vbool(fail_errno().t);
    return vbool(1);
}

static Value b_accept(Value *a, int n)
{
    (void)n;
    int fd = accept((int)need_int(a[0], "accept"), NULL, NULL);
    if (fd < 0)
        last_errno = errno;
    return vint(fd);
}

/* -- time -- */

static Value tm_map(struct tm *tm)
{
    Map *m = map_new();
    map_sets(m, "year", vint(tm->tm_year + 1900));
    map_sets(m, "mon", vint(tm->tm_mon + 1));
    map_sets(m, "mday", vint(tm->tm_mday));
    map_sets(m, "hour", vint(tm->tm_hour));
    map_sets(m, "min", vint(tm->tm_min));
    map_sets(m, "sec", vint(tm->tm_sec));
    map_sets(m, "wday", vint(tm->tm_wday));
    map_sets(m, "yday", vint(tm->tm_yday));
    map_sets(m, "isdst", vint(tm->tm_isdst));
    return vmap(m);
}

static void map_tm(Map *m, struct tm *tm)
{
    memset(tm, 0, sizeof *tm);
    Value v;
    tm->tm_year = (v = map_gets(m, "year")).t == T_INT ? (int)v.u.i - 1900 : 70;
    tm->tm_mon = (v = map_gets(m, "mon")).t == T_INT ? (int)v.u.i - 1 : 0;
    tm->tm_mday = (v = map_gets(m, "mday")).t == T_INT ? (int)v.u.i : 1;
    tm->tm_hour = (v = map_gets(m, "hour")).t == T_INT ? (int)v.u.i : 0;
    tm->tm_min = (v = map_gets(m, "min")).t == T_INT ? (int)v.u.i : 0;
    tm->tm_sec = (v = map_gets(m, "sec")).t == T_INT ? (int)v.u.i : 0;
    tm->tm_isdst = (v = map_gets(m, "isdst")).t == T_INT ? (int)v.u.i : -1;
}

static Value b_gmtime(Value *a, int n)
{
    time_t t = n ? (time_t)need_int(a[0], "gmtime") : time(NULL);
    struct tm *tm = gmtime(&t);
    return tm ? tm_map(tm) : NIL;
}

static Value b_localtime(Value *a, int n)
{
    time_t t = n ? (time_t)need_int(a[0], "localtime") : time(NULL);
    tzset();
    struct tm *tm = localtime(&t);
    return tm ? tm_map(tm) : NIL;
}

/* timegm(map): the seconds since the Epoch of a time in UTC */
static Value b_timegm(Value *a, int n)
{
    (void)n;
    struct tm tm;
    map_tm(need_map(a[0], "timegm"), &tm);
    int64_t y = tm.tm_year + 1900, mo = tm.tm_mon + 1, d = tm.tm_mday;
    /* days from the civil date (Howard Hinnant's algorithm) */
    y -= mo <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = era * 146097 + doe - 719468;
    return vint(days * 86400 + tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
}

static Value b_mktime(Value *a, int n)
{
    (void)n;
    struct tm tm;
    map_tm(need_map(a[0], "mktime"), &tm);
    tzset();
    return vint((int64_t)mktime(&tm));
}

static Value b_strftime(Value *a, int n)
{
    const char *f = need_str(a[0], "strftime")->p;
    time_t t = OPT(1) ? (time_t)need_int(a[1], "strftime") : time(NULL);
    tzset();
    struct tm *tm = OPT(2) && truth(a[2]) ? gmtime(&t) : localtime(&t);
    char buf[1024];
    if (!tm)
        return NIL;
    size_t k = strftime(buf, sizeof buf, f, tm);
    return vstrn(buf, k);
}

static Value b_strptime(Value *a, int n)
{
    (void)n;
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_mday = 1;
    const char *s = need_str(a[0], "strptime")->p;
    const char *e = strptime(s, need_str(a[1], "strptime")->p, &tm);
    if (!e)
        return NIL;
    tm.tm_isdst = -1;
    return tm_map(&tm);
}

/* -- binary data: pack and unpack by a format of sizes and byte order -- */

static int pack_size(char c)
{
    switch (c) {
    case 'b': case 'B': case 'x': case 's': return 1;
    case 'h': case 'H': return 2;
    case 'i': case 'I': return 4;
    case 'q': case 'Q': return 8;
    }
    return 0;
}

static Value b_pack(Value *a, int n)
{
    const char *f = need_str(a[0], "pack")->p;
    int big = 0, ai = 1;
    {
        uint16_t one = 1;
        big = *(unsigned char *)&one == 0;
    }
    Str *b = buf_new();
    for (const char *p = f; *p; p++) {
        if (*p == '<') {
            big = 0;
            continue;
        }
        if (*p == '>' || *p == '!') {
            big = 1;
            continue;
        }
        if (*p == '=' || *p == ' ')
            continue;
        int count = 1;
        if (isdigit((unsigned char)*p)) {
            count = 0;
            while (isdigit((unsigned char)*p))
                count = count * 10 + *p++ - '0';
        }
        int sz = pack_size(*p);
        if (!sz)
            die("pack: unknown format character %c", *p);
        if (*p == 's') {
            if (ai >= n)
                die("pack: too few values");
            Str *s = need_str(a[ai++], "pack");
            for (int i = 0; i < count; i++)
                str_putc(b, (size_t)i < s->len ? s->p[i] : 0);
            continue;
        }
        for (int k = 0; k < count; k++) {
            if (*p == 'x') {
                str_putc(b, 0);
                continue;
            }
            if (ai >= n)
                die("pack: too few values");
            uint64_t v = (uint64_t)need_int(a[ai++], "pack");
            for (int i = 0; i < sz; i++) {
                int sh = big ? (sz - 1 - i) * 8 : i * 8;
                str_putc(b, (int)((v >> sh) & 0xff));
            }
        }
    }
    if (ai < n)
        die("pack: more values than the format takes");
    return vstr_take(b);
}

static Value b_unpack(Value *a, int n)
{
    const char *f = need_str(a[0], "unpack")->p;
    Str *s = need_str(a[1], "unpack");
    size_t off = OPT(2) ? (size_t)need_int(a[2], "unpack") : 0;
    int big;
    {
        uint16_t one = 1;
        big = *(unsigned char *)&one == 0;
    }
    List *l = list_new(4);
    for (const char *p = f; *p; p++) {
        if (*p == '<') {
            big = 0;
            continue;
        }
        if (*p == '>' || *p == '!') {
            big = 1;
            continue;
        }
        if (*p == '=' || *p == ' ')
            continue;
        int count = 1;
        if (isdigit((unsigned char)*p)) {
            count = 0;
            while (isdigit((unsigned char)*p))
                count = count * 10 + *p++ - '0';
        }
        int sz = pack_size(*p);
        if (!sz)
            die("unpack: unknown format character %c", *p);
        if (*p == 's') {
            if (off + (size_t)count > s->len)
                die("unpack: the data is too short");
            list_push(l, vstrn(s->p + off, (size_t)count));
            off += (size_t)count;
            continue;
        }
        for (int k = 0; k < count; k++) {
            if (off + (size_t)sz > s->len)
                die("unpack: the data is too short");
            if (*p == 'x') {
                off++;
                continue;
            }
            uint64_t v = 0;
            for (int i = 0; i < sz; i++) {
                int sh = big ? (sz - 1 - i) * 8 : i * 8;
                v |= (uint64_t)(unsigned char)s->p[off + (size_t)i] << sh;
            }
            off += (size_t)sz;
            int64_t sv = (int64_t)v;
            if (islower((unsigned char)*p) && sz < 8 && (v >> (sz * 8 - 1)) & 1)
                sv = (int64_t)(v | (~(uint64_t)0 << (sz * 8)));
            list_push(l, vint(sv));
        }
    }
    return vlist(l);
}

/* ---- terminals: a program on a pseudo-terminal, and a screen ---- */

typedef struct Cell {
    char b[4];
    unsigned char n;            /* 0: a blank */
    unsigned char attr;         /* A_BOLD, A_UNDER, A_REVERSE */
} Cell;

#define A_BOLD 1
#define A_UNDER 2
#define A_REVERSE 4

struct Term {
    pid_t pid;
    int master, eof, status;    /* status -1: running */
    int sig;                    /* the signal that ended it */
    int rows, cols;
    Cell *cells;
    int r, c;                   /* c == cols: a wrap is pending */
    int top, bot;               /* the scrolling region */
    int sr, sc;                 /* the saved cursor */
    int autowrap, insert;
    int attr;                   /* the attributes of what is drawn */
    int bells;
    Str *raw;                   /* all the output */
    Str *pend;                  /* an unfinished sequence */
    int pipefd;                 /* the standard output, "pipe" */
    Str *piped;
};

static Cell *cell(Term *t, int r, int c)
{
    return &t->cells[(size_t)r * (size_t)t->cols + (size_t)c];
}

static void blank_cells(Term *t, int r, int c0, int c1)
{
    for (int c = c0; c < c1; c++) {
        cell(t, r, c)->n = 0;
        cell(t, r, c)->attr = 0;
    }
}

static void blank_rows(Term *t, int r0, int r1)
{
    for (int r = r0; r < r1; r++)
        blank_cells(t, r, 0, t->cols);
}

static void scroll_up(Term *t, int top, int bot, int n)
{
    if (n > bot - top + 1)
        n = bot - top + 1;
    size_t w = (size_t)t->cols * sizeof(Cell);
    memmove(cell(t, top, 0), cell(t, top + n, 0), (size_t)(bot - top + 1 - n) * w);
    blank_rows(t, bot - n + 1, bot + 1);
}

static void scroll_down(Term *t, int top, int bot, int n)
{
    if (n > bot - top + 1)
        n = bot - top + 1;
    size_t w = (size_t)t->cols * sizeof(Cell);
    memmove(cell(t, top + n, 0), cell(t, top, 0), (size_t)(bot - top + 1 - n) * w);
    blank_rows(t, top, top + n);
}

static void t_index(Term *t)
{
    if (t->r == t->bot)
        scroll_up(t, t->top, t->bot, 1);
    else if (t->r < t->rows - 1)
        t->r++;
}

static void t_revindex(Term *t)
{
    if (t->r == t->top)
        scroll_down(t, t->top, t->bot, 1);
    else if (t->r > 0)
        t->r--;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void t_draw(Term *t, const char *b, int n)
{
    if (t->c >= t->cols) {
        if (t->autowrap) {
            t->c = 0;
            t_index(t);
        } else
            t->c = t->cols - 1;
    }
    if (t->insert) {
        Cell *row = cell(t, t->r, 0);
        memmove(&row[t->c + 1], &row[t->c], (size_t)(t->cols - t->c - 1) * sizeof(Cell));
    }
    Cell *ce = cell(t, t->r, t->c);
    memcpy(ce->b, b, (size_t)n);
    ce->n = (unsigned char)n;
    ce->attr = (unsigned char)t->attr;
    t->c++;
}

static void t_csi(Term *t, char priv, int *p, int np, char fin)
{
    int p0 = np > 0 ? p[0] : 0, n1 = p0 > 0 ? p0 : 1;
    int cc = t->c >= t->cols ? t->cols - 1 : t->c;
    switch (fin) {
    case 'A': t->r = clampi(t->r - n1, 0, t->rows - 1); t->c = cc; break;
    case 'B': case 'e': t->r = clampi(t->r + n1, 0, t->rows - 1); t->c = cc; break;
    case 'C': case 'a': t->c = clampi(cc + n1, 0, t->cols - 1); break;
    case 'D': t->c = clampi(cc - n1, 0, t->cols - 1); break;
    case 'E': t->r = clampi(t->r + n1, 0, t->rows - 1); t->c = 0; break;
    case 'F': t->r = clampi(t->r - n1, 0, t->rows - 1); t->c = 0; break;
    case 'G': case '`': t->c = clampi(n1 - 1, 0, t->cols - 1); break;
    case 'd': t->r = clampi(n1 - 1, 0, t->rows - 1); t->c = cc; break;
    case 'H': case 'f':
        t->r = clampi((np > 0 && p[0] > 0 ? p[0] : 1) - 1, 0, t->rows - 1);
        t->c = clampi((np > 1 && p[1] > 0 ? p[1] : 1) - 1, 0, t->cols - 1);
        break;
    case 'J':
        if (p0 == 0) {
            blank_cells(t, t->r, cc, t->cols);
            blank_rows(t, t->r + 1, t->rows);
        } else if (p0 == 1) {
            blank_rows(t, 0, t->r);
            blank_cells(t, t->r, 0, cc + 1);
        } else
            blank_rows(t, 0, t->rows);
        break;
    case 'K':
        if (p0 == 0)
            blank_cells(t, t->r, cc, t->cols);
        else if (p0 == 1)
            blank_cells(t, t->r, 0, cc + 1);
        else
            blank_cells(t, t->r, 0, t->cols);
        break;
    case 'L':
        if (t->r >= t->top && t->r <= t->bot) {
            scroll_down(t, t->r, t->bot, n1);
            t->c = 0;
        }
        break;
    case 'M':
        if (t->r >= t->top && t->r <= t->bot) {
            scroll_up(t, t->r, t->bot, n1);
            t->c = 0;
        }
        break;
    case 'P': {
        Cell *row = cell(t, t->r, 0);
        int k = n1 > t->cols - cc ? t->cols - cc : n1;
        memmove(&row[cc], &row[cc + k], (size_t)(t->cols - cc - k) * sizeof(Cell));
        blank_cells(t, t->r, t->cols - k, t->cols);
        break;
    }
    case '@': {
        Cell *row = cell(t, t->r, 0);
        int k = n1 > t->cols - cc ? t->cols - cc : n1;
        memmove(&row[cc + k], &row[cc], (size_t)(t->cols - cc - k) * sizeof(Cell));
        blank_cells(t, t->r, cc, cc + k);
        break;
    }
    case 'X':
        blank_cells(t, t->r, cc, cc + n1 > t->cols ? t->cols : cc + n1);
        break;
    case 'S': scroll_up(t, t->top, t->bot, n1); break;
    case 'T': if (!priv) scroll_down(t, t->top, t->bot, n1); break;
    case 'r': {
        int top = np > 0 && p[0] > 0 ? p[0] : 1, bot = np > 1 && p[1] > 0 ? p[1] : t->rows;
        if (top < bot && bot <= t->rows) {
            t->top = top - 1;
            t->bot = bot - 1;
            t->r = 0;
            t->c = 0;
        }
        break;
    }
    case 'h': case 'l':
        for (int i = 0; i < (np ? np : 1); i++) {
            int m = np ? p[i] : 0;
            if (priv == '?' && m == 7)
                t->autowrap = fin == 'h';
            else if (!priv && m == 4)
                t->insert = fin == 'h';
        }
        break;
    case 's': t->sr = t->r; t->sc = cc; break;
    case 'u': t->r = t->sr; t->c = t->sc; break;
    case 'm':
        if (priv)
            break;
        for (int i = 0; i < (np ? np : 1); i++)
            switch (np ? p[i] : 0) {
            case 0: t->attr = 0; break;
            case 1: t->attr |= A_BOLD; break;
            case 4: t->attr |= A_UNDER; break;
            case 7: t->attr |= A_REVERSE; break;
            case 22: t->attr &= ~A_BOLD; break;
            case 24: t->attr &= ~A_UNDER; break;
            case 27: t->attr &= ~A_REVERSE; break;
            default: break;     /* colours */
            }
        break;
    default: break;
    }
}

/* the bytes of the output, through the screen */
static void t_feed(Term *t, const char *data, size_t len)
{
    str_append(t->raw, data, len);
    str_append(t->pend, data, len);
    const unsigned char *s = (const unsigned char *)t->pend->p;
    size_t n = t->pend->len, i = 0;
    while (i < n) {
        unsigned char b = s[i];
        if (b == 033) {
            if (i + 1 >= n)
                break;
            unsigned char k = s[i + 1];
            if (k == '[') {
                size_t j = i + 2;
                char priv = 0;
                if (j < n && (s[j] == '?' || s[j] == '>' || s[j] == '=' || s[j] == '!'))
                    priv = (char)s[j++];
                int p[16], np = 0, cur = -1;
                while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == ';' || s[j] == ':')) {
                    if (s[j] == ';' || s[j] == ':') {
                        if (np < 16)
                            p[np++] = cur < 0 ? 0 : cur;
                        cur = -1;
                    } else
                        cur = (cur < 0 ? 0 : cur) * 10 + (s[j] - '0');
                    j++;
                }
                while (j < n && s[j] >= 0x20 && s[j] <= 0x2f)
                    j++;
                if (j >= n)
                    break;          /* unfinished */
                if (cur >= 0 && np < 16)
                    p[np++] = cur;
                t_csi(t, priv, p, np, (char)s[j]);
                i = j + 1;
                continue;
            }
            if (k == ']' || k == 'P' || k == '_' || k == '^') {
                /* a string: up to BEL or ST */
                size_t j = i + 2;
                while (j < n && s[j] != 7 && !(s[j] == 033 && j + 1 < n && s[j + 1] == '\\'))
                    j++;
                if (j >= n)
                    break;
                i = j + (s[j] == 7 ? 1 : 2);
                continue;
            }
            if (k == '(' || k == ')' || k == '*' || k == '+' || k == '#' || k == '%') {
                if (i + 2 >= n)
                    break;
                i += 3;
                continue;
            }
            switch (k) {
            case '7': t->sr = t->r; t->sc = t->c >= t->cols ? t->cols - 1 : t->c; break;
            case '8': t->r = t->sr; t->c = t->sc; break;
            case 'D': t_index(t); break;
            case 'E': t->c = 0; t_index(t); break;
            case 'M': t_revindex(t); break;
            case 'c':
                blank_rows(t, 0, t->rows);
                t->r = t->c = 0;
                t->top = 0;
                t->bot = t->rows - 1;
                t->autowrap = 1;
                t->insert = 0;
                t->attr = 0;
                break;
            default: break;
            }
            i += 2;
            continue;
        }
        if (b < 0x20 || b == 0x7f) {
            switch (b) {
            case 7: t->bells++; break;
            case 8: t->c = clampi((t->c >= t->cols ? t->cols - 1 : t->c) - 1, 0, t->cols - 1); break;
            case 9: {
                int c = t->c >= t->cols ? t->cols - 1 : t->c;
                c = (c / 8 + 1) * 8;
                t->c = c >= t->cols ? t->cols - 1 : c;
                break;
            }
            case 10: case 11: case 12: t_index(t); break;
            case 13: t->c = 0; break;
            default: break;
            }
            i++;
            continue;
        }
        int cl = 1;
        if (b >= 0xc0)
            cl = b < 0xe0 ? 2 : b < 0xf0 ? 3 : 4;
        if (i + (size_t)cl > n)
            break;
        for (int k = 1; k < cl; k++)
            if ((s[i + (size_t)k] & 0xc0) != 0x80)
                cl = 1;
        t_draw(t, (const char *)s + i, cl);
        i += (size_t)cl;
    }
    /* keep what is not finished */
    memmove(t->pend->p, t->pend->p + i, n - i);
    t->pend->len = n - i;
    t->pend->p[t->pend->len] = '\0';
    if (t->pend->len > 4096)
        t->pend->len = 0;           /* not a sequence after all */
}

static Term *need_term(Value v, const char *what)
{
    if (v.t != T_TERM)
        die("%s: a terminal expected, not %s", what, type_name(v.t));
    return v.u.term;
}

static int termios_char(Map *tm, const char *name, cc_t *slot)
{
    Value v = map_gets(tm, name);
    if (v.t == T_INT) {
        *slot = (cc_t)v.u.i;
        return 1;
    }
    if (v.t == T_STR && v.u.s->len == 1) {
        *slot = (cc_t)v.u.s->p[0];
        return 1;
    }
    return 0;
}

static void termios_flag(Map *tm, const char *name, tcflag_t *flags, tcflag_t bit)
{
    Value v = map_gets(tm, name);
    if (v.t == T_NIL)
        return;
    if (truth(v))
        *flags |= bit;
    else
        *flags &= ~bit;
}

static void set_winsize(int fd, int rows, int cols)
{
    struct winsize ws;
    memset(&ws, 0, sizeof ws);
    ws.ws_row = (unsigned short)rows;
    ws.ws_col = (unsigned short)cols;
    tcsetwinsize(fd, &ws);
}

/*
 * term(cmd[, opts]): a program on a new pseudo-terminal, its controlling
 * terminal (in a new session).  cmd: a list (the program and its
 * arguments) or a string (sh -c).  opts: rows, cols (24, 80), fds (those
 * of 0, 1, 2 on the terminal: "012"), in, out, err (the others:
 * "inherit", the default; "null"; "pipe" for out: tpiped()), env, cwd,
 * umask, termios (a map: erase, kill, werase, eof, intr, quit, susp:
 * characters; icanon, echo, isig, iexten: true or false).
 */
static Value b_term(Value *a, int n)
{
    Map *opts = OPT(1) ? need_map(a[1], "term") : NULL;
    char **argv = argv_of(a[0], "term");
    int rows = (int)opt_num(opts, "rows", 24), cols = (int)opt_num(opts, "cols", 80);
    int nosize = rows <= 0 || cols <= 0;   /* the terminal's size left 0 (the screen 24 by 80) */
    if (nosize) {
        rows = 24;
        cols = 80;
    }
    const char *fds = opt_str(opts, "fds", "012");
    int m = posix_openpt(O_RDWR | O_NOCTTY);
    if (m < 0 || grantpt(m) < 0 || unlockpt(m) < 0)
        die("term: no pseudo-terminal: %s", strerror(errno));
    char *sname = xstrdup(ptsname(m));
    set_cloexec(m);
    if (!nosize)
        set_winsize(m, rows, cols);
    int pp[2] = { -1, -1 };
    if (!strchr(fds, '1') && strcmp(opt_str(opts, "out", "inherit"), "pipe") == 0) {
        if (pipe(pp) < 0)
            die("term: pipe: %s", strerror(errno));
        set_cloexec(pp[0]);
    }
    flush_all();
    pid_t pid = fork();
    if (pid < 0)
        die("term: fork: %s", strerror(errno));
    if (pid == 0) {
        /* a session leader without a controlling terminal: opening the
           terminal (without O_NOCTTY) makes it the controlling terminal */
        setsid();
        int s = open(sname, O_RDWR);
        if (s < 0) {
            fprintf(stderr, "tl: %s: %s\n", sname, strerror(errno));
            _exit(127);
        }
        if (!nosize)
            set_winsize(s, rows, cols);
        Value tmv = opts ? map_gets(opts, "termios") : NIL;
        if (tmv.t == T_MAP) {
            struct termios tio;
            tcgetattr(s, &tio);
            Map *tm = tmv.u.m;
            termios_char(tm, "erase", &tio.c_cc[VERASE]);
            termios_char(tm, "kill", &tio.c_cc[VKILL]);
#ifdef VWERASE
            termios_char(tm, "werase", &tio.c_cc[VWERASE]);
#endif
            termios_char(tm, "eof", &tio.c_cc[VEOF]);
            termios_char(tm, "intr", &tio.c_cc[VINTR]);
            termios_char(tm, "quit", &tio.c_cc[VQUIT]);
            termios_char(tm, "susp", &tio.c_cc[VSUSP]);
            termios_flag(tm, "icanon", &tio.c_lflag, ICANON);
            termios_flag(tm, "echo", &tio.c_lflag, ECHO);
            termios_flag(tm, "isig", &tio.c_lflag, ISIG);
            termios_flag(tm, "iexten", &tio.c_lflag, IEXTEN);
            termios_flag(tm, "onlcr", &tio.c_oflag, ONLCR);
            tcsetattr(s, TCSANOW, &tio);
        }
        static const char *const names[] = { "in", "out", "err" };
        for (int fd = 0; fd < 3; fd++) {
            if (strchr(fds, '0' + fd))
                dup2(s, fd);
            else if (fd == 1 && pp[1] >= 0) {
                dup2(pp[1], 1);
                close(pp[0]);
                close(pp[1]);
            } else {
                int f = open_disp(opt_str(opts, names[fd], "inherit"), fd);
                if (f >= 0) {
                    dup2(f, fd);
                    close(f);
                }
            }
        }
        if (s > 2)
            close(s);
        child_setup(opts);
        signal(SIGPIPE, SIG_DFL);
        exec_argv(argv, a[0].t == T_STR);
    }
    free(sname);
    if (pp[1] >= 0)
        close(pp[1]);
    if (a[0].t != T_STR)
        for (char **p = argv; *p; p++)
            free(*p);
    free(argv);
    fcntl(m, F_SETFL, fcntl(m, F_GETFL) | O_NONBLOCK);
    Term *t = xcalloc(1, sizeof *t);
    t->pid = pid;
    t->master = m;
    t->status = -1;
    t->rows = rows;
    t->cols = cols;
    t->cells = xcalloc((size_t)rows * (size_t)cols, sizeof(Cell));
    t->bot = rows - 1;
    t->autowrap = 1;
    t->raw = buf_new();
    t->pend = buf_new();
    t->pipefd = pp[0];
    t->piped = buf_new();
    Value v;
    v.t = T_TERM;
    v.u.term = t;
    return v;
}

/* reads what is there, waiting up to secs for something; 1 if it read */
static int t_read(Term *t, double secs)
{
    struct pollfd pf[2];
    int np = 0, mi = -1, pi = -1;
    if (!t->eof) {
        pf[np].fd = t->master;
        pf[np].events = POLLIN;
        mi = np++;
    }
    if (t->pipefd >= 0) {
        pf[np].fd = t->pipefd;
        pf[np].events = POLLIN;
        pi = np++;
    }
    if (!np) {
        if (secs > 0) {
            struct timespec ts = { (time_t)secs, (long)((secs - (double)(time_t)secs) * 1e9) };
            nanosleep(&ts, NULL);
        }
        return 0;
    }
    int r = poll(pf, (nfds_t)np, secs > 0 ? (int)(secs * 1000) : 0);
    if (r <= 0)
        return 0;
    int got = 0;
    char buf[65536];
    if (mi >= 0 && pf[mi].revents) {
        ssize_t k = read(t->master, buf, sizeof buf);
        if (k > 0) {
            t_feed(t, buf, (size_t)k);
            got = 1;
        } else if (k == 0 || (errno != EAGAIN && errno != EINTR))
            t->eof = 1;
    }
    if (pi >= 0 && pf[pi].revents) {
        ssize_t k = read(t->pipefd, buf, sizeof buf);
        if (k > 0) {
            str_append(t->piped, buf, (size_t)k);
            got = 1;
        } else if (k == 0 || (errno != EAGAIN && errno != EINTR)) {
            close(t->pipefd);
            t->pipefd = -1;
        }
    }
    return got;
}

/* tpump(t[, secs]): reads the output for secs seconds (0: what is there) */
static Value b_tpump(Value *a, int n)
{
    Term *t = need_term(a[0], "tpump");
    double secs = OPT(1) ? need_num(a[1], "tpump") : 0;
    double end = now_secs() + secs;
    for (;;) {
        double left = end - now_secs();
        if (left <= 0) {
            while (t_read(t, 0))
                ;
            break;
        }
        t_read(t, left);
        if (t->eof && t->pipefd < 0)
            break;          /* nothing more can come */
    }
    return NIL;
}

/* tsettle(t[, quiet[, max]]): reads until there has been no output for
   quiet seconds (0.15), or for max seconds (5) */
static Value b_tsettle(Value *a, int n)
{
    Term *t = need_term(a[0], "tsettle");
    double quiet = OPT(1) ? need_num(a[1], "tsettle") : 0.15, max = OPT(2) ? need_num(a[2], "tsettle") : 5;
    double start = now_secs(), last = start;
    while (now_secs() - start < max) {
        if (t_read(t, 0.02))
            last = now_secs();
        else if (now_secs() - last > quiet || (t->eof && t->pipefd < 0))
            break;
    }
    return NIL;
}

static Value b_tsend(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tsend");
    Str *s = need_str(a[1], "tsend");
    size_t off = 0;
    double end = now_secs() + 5;
    while (off < s->len && now_secs() < end) {
        ssize_t w = write(t->master, s->p + off, s->len - off);
        if (w > 0)
            off += (size_t)w;
        else if (w < 0 && errno != EAGAIN && errno != EINTR)
            break;
        else
            t_read(t, 0.01);    /* the program may be waiting to write */
    }
    return vbool(off == s->len);
}

static int t_poll_exit(Term *t)
{
    if (t->status >= 0)
        return 1;
    int st;
    pid_t r = waitpid(t->pid, &st, WNOHANG);
    if (r == t->pid) {
        t->status = (int)status_of(st);
        t->sig = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
        return 1;
    }
    return 0;
}

/* twait(t[, secs]): waits up to secs (5) for the program to end, reading
   its output: its exit status, or nil */
static Value b_twait(Value *a, int n)
{
    Term *t = need_term(a[0], "twait");
    double end = now_secs() + (OPT(1) ? need_num(a[1], "twait") : 5);
    for (;;) {
        if (t_poll_exit(t)) {
            /* the rest of the output */
            double e2 = now_secs() + 0.5;
            while (now_secs() < e2 && (!t->eof || t->pipefd >= 0))
                if (!t_read(t, 0.05))
                    break;
            return vint(t->status);
        }
        if (now_secs() >= end)
            return NIL;
        t_read(t, 0.05);
    }
}

/* teof(t): the terminal has no writer left (and the pipe of out is at its end) */
static Value b_teof(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "teof");
    return vbool(t->eof && t->pipefd < 0);
}

static Value b_talive(Value *a, int n)
{
    (void)n;
    return vbool(!t_poll_exit(need_term(a[0], "talive")));
}

static Value b_tkill(Value *a, int n)
{
    Term *t = need_term(a[0], "tkill");
    int sig = OPT(1) ? (int)need_int(a[1], "tkill") : SIGKILL;
    if (!t_poll_exit(t)) {
        kill(t->pid, sig);
        if (sig == SIGKILL) {
            int st;
            waitpid(t->pid, &st, 0);
            t->status = (int)status_of(st);
            t->sig = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
        }
    }
    return NIL;
}

static Value line_value(Term *t, int r)
{
    Str *b = buf_new();
    for (int c = 0; c < t->cols; c++) {
        Cell *ce = cell(t, r, c);
        if (ce->n)
            str_append(b, ce->b, ce->n);
        else
            str_putc(b, ' ');
    }
    while (b->len && b->p[b->len - 1] == ' ')
        b->len--;
    b->p[b->len] = '\0';
    return vstr_take(b);
}

static Value b_tline(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tline");
    int64_t r = need_int(a[1], "tline");
    if (r < 0)
        r += t->rows;
    if (r < 0 || r >= t->rows)
        die("tline: row %lld out of range", (long long)r);
    return line_value(t, (int)r);
}

static Value b_tscreen(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tscreen");
    Str *b = buf_new();
    for (int r = 0; r < t->rows; r++) {
        Value l = line_value(t, r);
        if (r)
            str_putc(b, '\n');
        str_append(b, l.u.s->p, l.u.s->len);
        decref(l);
    }
    return vstr_take(b);
}

/* tcell(t, row, col): a cell of the screen: {ch, bold, underline, reverse} */
static Value b_tcell(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tcell");
    int64_t r = need_int(a[1], "tcell"), c = need_int(a[2], "tcell");
    if (r < 0)
        r += t->rows;
    if (r < 0 || r >= t->rows || c < 0 || c >= t->cols)
        die("tcell: %lld, %lld out of range", (long long)r, (long long)c);
    Cell *ce = cell(t, (int)r, (int)c);
    Map *m = map_new();
    map_sets(m, "ch", ce->n ? vstrn(ce->b, ce->n) : vstr(""));
    map_sets(m, "bold", vbool(ce->attr & A_BOLD));
    map_sets(m, "underline", vbool(ce->attr & A_UNDER));
    map_sets(m, "reverse", vbool(ce->attr & A_REVERSE));
    return vmap(m);
}

/* tsignal(t): the signal that ended the program, 0 if it exited */
static Value b_tsignal(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tsignal");
    t_poll_exit(t);
    return vint(t->sig);
}

static Value b_trow(Value *a, int n) { (void)n; return vint(need_term(a[0], "trow")->r); }

static Value b_tcol(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tcol");
    return vint(t->c >= t->cols ? t->cols - 1 : t->c);
}

static Value b_tbells(Value *a, int n) { (void)n; return vint(need_term(a[0], "tbells")->bells); }
static Value b_tpid(Value *a, int n) { (void)n; return vint(need_term(a[0], "tpid")->pid); }
static Value b_tfd(Value *a, int n) { (void)n; return vint(need_term(a[0], "tfd")->master); }

static Value b_traw(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "traw");
    return vstrn(t->raw->p, t->raw->len);
}

static Value b_tpiped(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tpiped");
    return vstrn(t->piped->p, t->piped->len);
}

/* tfill(t, ch): every cell of the screen made ch */
static Value b_tfill(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tfill");
    Str *s = need_str(a[1], "tfill");
    for (int r = 0; r < t->rows; r++)
        for (int c = 0; c < t->cols; c++) {
            Cell *ce = cell(t, r, c);
            ce->n = (unsigned char)(s->len > 4 ? 4 : s->len);
            memcpy(ce->b, s->p, ce->n);
        }
    return NIL;
}

static Value b_tresize(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tresize");
    int rows = (int)need_int(a[1], "tresize"), cols = (int)need_int(a[2], "tresize");
    Cell *nc = xcalloc((size_t)rows * (size_t)cols, sizeof(Cell));
    for (int r = 0; r < rows && r < t->rows; r++)
        memcpy(&nc[(size_t)r * (size_t)cols], cell(t, r, 0), (size_t)(cols < t->cols ? cols : t->cols) * sizeof(Cell));
    free(t->cells);
    t->cells = nc;
    t->rows = rows;
    t->cols = cols;
    t->top = 0;
    t->bot = rows - 1;
    t->r = clampi(t->r, 0, rows - 1);
    t->c = clampi(t->c, 0, cols - 1);
    set_winsize(t->master, rows, cols);
#ifdef SIGWINCH
    kill(t->pid, SIGWINCH);
#endif
    return NIL;
}

/* tattr(t): the modes of the terminal now (icanon, echo, isig, iexten) */
static Value b_tattr(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tattr");
    struct termios tio;
    if (tcgetattr(t->master, &tio) < 0)
        return fail_errno();
    Map *m = map_new();
    map_sets(m, "icanon", vbool(tio.c_lflag & ICANON));
    map_sets(m, "echo", vbool(tio.c_lflag & ECHO));
    map_sets(m, "isig", vbool(tio.c_lflag & ISIG));
    map_sets(m, "iexten", vbool(tio.c_lflag & IEXTEN));
    return vmap(m);
}

static Value b_tclose(Value *a, int n)
{
    (void)n;
    Term *t = need_term(a[0], "tclose");
    if (t->master >= 0)
        close(t->master);
    t->master = -1;
    t->eof = 1;
    return NIL;
}

/* ---- the table ---- */

static const Builtin builtins[] = {
    { "print", b_print, 0, -1 }, { "printf", b_printf, 1, -1 }, { "eprintf", b_eprintf, 1, -1 },
    { "sprintf", b_sprintf, 1, -1 }, { "puts", b_puts, 1, 1 }, { "eputs", b_eputs, 1, 1 },
    { "flush", b_flush, 0, 0 }, { "exit", b_exit, 0, 1 }, { "error", b_error, 1, -1 },
    { "assert", b_assert, 1, 2 },
    { "str", b_str, 1, 1 }, { "repr", b_repr, 1, 1 }, { "typeof", b_typeof, 1, 1 }, { "nameof", b_nameof, 1, 1 }, { "len", b_len, 1, 1 },
    { "int", b_int, 1, 2 }, { "num", b_num, 1, 1 }, { "double", b_double, 1, 1 },
    { "substr", b_substr, 2, 3 }, { "slice", b_slice, 2, 3 }, { "index", b_index, 2, 3 },
    { "rindex", b_rindex, 2, 2 }, { "startswith", b_startswith, 2, 2 }, { "endswith", b_endswith, 2, 2 },
    { "split", b_split, 1, 3 }, { "lines", b_lines, 1, 1 }, { "join", b_join, 1, 2 },
    { "replace", b_replace, 3, 4 }, { "trim", b_trim, 1, 2 }, { "ltrim", b_ltrim, 1, 2 },
    { "rtrim", b_rtrim, 1, 2 }, { "upper", b_upper, 1, 1 }, { "lower", b_lower, 1, 1 },
    { "repeat", b_repeat, 2, 2 }, { "chr", b_chr, 1, 1 }, { "utf8", b_utf8, 1, 1 }, { "ord", b_ord, 1, 2 },
    { "hex", b_hex, 1, 1 }, { "unhex", b_unhex, 1, 1 }, { "json", b_json, 1, 2 }, { "unjson", b_unjson, 1, 1 },
    { "base64", b_base64, 1, 1 }, { "unbase64", b_unbase64, 1, 1 }, { "sha256", b_sha256, 1, 1 },
    { "match", b_match, 2, 3 }, { "matchat", b_matchat, 2, 3 }, { "sub", b_sub, 3, 4 },
    { "gsub", b_gsub, 3, 4 }, { "resplit", b_resplit, 2, 3 },
    { "list", b_list, 1, 2 }, { "range", b_range, 1, 3 }, { "push", b_push, 1, -1 }, { "pop", b_pop, 1, 2 },
    { "insert", b_insert, 3, 3 }, { "find", b_find, 2, 2 }, { "sort", b_sort, 1, 2 },
    { "sorted", b_sorted, 1, 2 }, { "reverse", b_reverse, 1, 1 }, { "copy", b_copy, 1, 1 },
    { "keys", b_keys, 1, 1 }, { "values", b_values, 1, 1 }, { "haskey", b_haskey, 2, 2 },
    { "del", b_del, 2, 2 }, { "get", b_get, 3, 3 }, { "apply", b_apply, 2, 2 }, { "filter", b_filter, 2, 2 },
    { "min", b_min, 1, -1 }, { "max", b_max, 1, -1 }, { "sum", b_sum, 1, 1 }, { "abs", b_abs, 1, 1 },
    { "srand", b_srand, 1, 1 }, { "rand", b_rand, 0, 2 }, { "frand", b_frand, 0, 0 },
    { "choice", b_choice, 1, 1 }, { "shuffle", b_shuffle, 1, 1 }, { "randbytes", b_randbytes, 1, 1 },
    { "readfile", b_readfile, 1, 1 }, { "writefile", b_writefile, 2, 3 }, { "appendfile", b_appendfile, 2, 3 },
    { "stat", b_stat, 1, 1 }, { "lstat", b_lstat, 1, 1 }, { "exists", b_exists, 1, 1 },
    { "isdir", b_isdir, 1, 1 }, { "isfile", b_isfile, 1, 1 }, { "remove", b_remove, 1, 1 },
    { "rmtree", b_rmtree, 1, 1 }, { "mkdir", b_mkdir, 1, 2 }, { "mkdirs", b_mkdirs, 1, 2 },
    { "listdir", b_listdir, 1, 1 }, { "rename", b_rename, 2, 2 }, { "symlink", b_symlink, 2, 2 },
    { "link", b_link, 2, 2 }, { "readlink", b_readlink, 1, 1 }, { "dirname", b_dirname, 1, 1 }, { "abspath", b_abspath, 1, 1 },
    { "basename", b_basename, 1, 1 }, { "chmod", b_chmod, 2, 2 },
    { "chown", b_chown, 3, 3 }, { "utime", b_utime, 3, 3 }, { "mkfifo", b_mkfifo, 1, 2 },
    { "access", b_access, 1, 2 }, { "getcwd", b_getcwd, 0, 0 }, { "chdir", b_chdir, 1, 1 },
    { "umask", b_umask, 1, 1 }, { "truncate", b_truncate, 2, 2 }, { "mkdtemp", b_mkdtemp, 0, 1 },
    { "errno", b_errno, 0, 0 }, { "strerror", b_strerror, 0, 1 },
    { "getenv", b_getenv, 1, 1 }, { "setenv", b_setenv, 2, 2 }, { "unsetenv", b_unsetenv, 1, 1 },
    { "environ", b_environ, 0, 0 }, { "setlocale", b_setlocale, 1, 2 }, { "iswctype", b_iswctype, 2, 2 },
    { "run", b_run, 1, 2 }, { "sh", b_sh, 1, 2 }, { "system", b_system, 1, 1 }, { "spawn", b_spawn, 1, 2 },
    { "waitpid", b_waitpid, 1, 2 }, { "termsig", b_termsig, 0, 0 }, { "fork", b_fork, 0, 0 }, { "exec", b_exec, 1, 1 },
    { "_exit", b__exit, 1, 1 },
    { "getpid", b_getpid, 0, 0 }, { "getppid", b_getppid, 0, 0 }, { "getpgrp", b_getpgrp, 0, 0 },
    { "setsid", b_setsid, 0, 0 }, { "getsid", b_getsid, 0, 1 }, { "setpgid", b_setpgid, 2, 2 }, { "kill", b_kill, 2, 2 },
    { "getuid", b_getuid, 0, 0 }, { "geteuid", b_geteuid, 0, 0 }, { "getgid", b_getgid, 0, 0 },
    { "getegid", b_getegid, 0, 0 }, { "setuid", b_setuid, 1, 1 }, { "seteuid", b_seteuid, 1, 1 },
    { "setgid", b_setgid, 1, 1 }, { "setegid", b_setegid, 1, 1 }, { "setreuid", b_setreuid, 2, 2 },
    { "setregid", b_setregid, 2, 2 }, { "setresuid", b_setresuid, 3, 3 }, { "setresgid", b_setresgid, 3, 3 },
    { "getgroups", b_getgroups, 0, 0 }, { "setgroups", b_setgroups, 1, 1 },
    { "getpwuid", b_getpwuid, 1, 1 }, { "getpwnam", b_getpwnam, 1, 1 }, { "getgrgid", b_getgrgid, 1, 1 },
    { "getgrnam", b_getgrnam, 1, 1 },
    { "signal", b_signal, 2, 2 }, { "sigdisp", b_sigdisp, 1, 1 }, { "sigmask", b_sigmask, 2, 2 },
    { "pause", b_pause, 0, 0 }, { "sleep", b_sleep, 1, 1 },
    { "open", b_open, 2, 3 }, { "close", b_close, 1, 1 }, { "dup2", b_dup2, 2, 2 }, { "pipe", b_pipe, 0, 0 },
    { "fdread", b_fdread, 1, 3 }, { "fdwrite", b_fdwrite, 2, 2 }, { "isatty", b_isatty, 1, 1 },
    { "ttyname", b_ttyname, 1, 1 }, { "mmap", b_mmap, 1, 1 }, { "unixconnect", b_unixconnect, 1, 1 },
    { "unixlisten", b_unixlisten, 1, 1 }, { "accept", b_accept, 1, 1 }, { "unixdgram", b_unixdgram, 1, 1 },
    { "lockf", b_lockf, 2, 2 },
    { "time", b_time, 0, 0 }, { "now", b_now, 0, 0 }, { "nice", b_nice, 1, 1 }, { "times", b_times, 0, 0 }, { "gmtime", b_gmtime, 0, 1 },
    { "localtime", b_localtime, 0, 1 }, { "timegm", b_timegm, 1, 1 }, { "mktime", b_mktime, 1, 1 },
    { "strftime", b_strftime, 1, 3 }, { "strptime", b_strptime, 2, 2 },
    { "pack", b_pack, 1, -1 }, { "unpack", b_unpack, 2, 3 },
    { "term", b_term, 1, 2 }, { "tpump", b_tpump, 1, 2 }, { "tsettle", b_tsettle, 1, 3 },
    { "tsend", b_tsend, 2, 2 }, { "twait", b_twait, 1, 2 }, { "talive", b_talive, 1, 1 }, { "teof", b_teof, 1, 1 },
    { "tkill", b_tkill, 1, 2 }, { "tcell", b_tcell, 3, 3 }, { "tline", b_tline, 2, 2 }, { "tscreen", b_tscreen, 1, 1 },
    { "trow", b_trow, 1, 1 }, { "tsignal", b_tsignal, 1, 1 }, { "tcol", b_tcol, 1, 1 }, { "tbells", b_tbells, 1, 1 }, { "tpid", b_tpid, 1, 1 },
    { "tfd", b_tfd, 1, 1 }, { "traw", b_traw, 1, 1 }, { "tpiped", b_tpiped, 1, 1 }, { "tfill", b_tfill, 2, 2 },
    { "tresize", b_tresize, 3, 3 }, { "tattr", b_tattr, 1, 1 }, { "tclose", b_tclose, 1, 1 },
    { NULL, NULL, 0, 0 }
};

NORETURN static void usage(void)
{
    fputs("usage: tl script [argument...]\n       tl -e program [argument...]\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    for (int i = 0; builtins[i].name; i++) {
        Value v;
        v.t = T_BUILTIN;
        v.u.b = &builtins[i];
        global_define(builtins[i].name, v);
    }
    for (int i = 0; signames[i].name; i++)
        global_define(signames[i].name, vint(signames[i].sig));
    signal(SIGPIPE, SIG_IGN);
    Parser p;
    memset(&p, 0, sizeof p);
    Node *prog = NULL;
    int argi = 0;
    const char *script = NULL;
    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        script = "-e";
        prog = parse_source(&p, "-e", xstrdup(argv[2]), strlen(argv[2]));
        argi = 3;
    } else if (argc >= 2) {
        if (argv[1][0] == '-' && argv[1][1])
            usage();
        script = argv[1];
        prog = parse_file(&p, script);
        argi = 2;
    } else
        usage();
    List *l = list_new(8);
    for (int i = argi; i < argc; i++)
        list_push(l, vstr(argv[i]));
    global_define("argv", vlist(l));
    global_define("script", vstr(script));
    {
        /* the directory of the script, absolute */
        const char *slash = strrchr(script, '/');
        Str *d = buf_new();
        if (script[0] != '/') {
            char cwd[PATH_MAX + 1];
            if (getcwd(cwd, sizeof cwd))
                str_append(d, cwd, strlen(cwd));
            if (slash) {
                str_putc(d, '/');
                str_append(d, script, (size_t)(slash - script));
            }
        } else
            str_append(d, script, slash == script ? 1 : (size_t)(slash - script));
        global_define("scriptdir", vstr_take(d));
    }
    exec(prog, NULL);
    flush_all();
    return 0;
}
