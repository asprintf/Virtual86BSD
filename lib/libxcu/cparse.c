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
 * cparse.c - a forgiving parser of C declarations and references, for
 * cflow, ctags and cxref
 *
 * The declarations are parsed (specifiers, declarators, initializers,
 * function definitions, K&R ones too, structure, union and enumeration
 * specifiers), with the scopes of the names, so that a typedef name is
 * told from an object; statements and expressions are only scanned for
 * the identifiers they use, the function calls (an identifier before
 * "(") and the assignments.  Whatever does not parse is skipped to the
 * next ";" or the end of the block.  A name that is not declared, before
 * another name (or at file scope before "*"), is taken as a type name:
 * that is what it most likely is in a file that is not preprocessed.
 */

#define _POSIX_C_SOURCE 202405L

#include "cparse.h"

#include <stdlib.h>
#include <string.h>

/* ---- scopes ---- */

#define NBUCKET 4093

struct entry {
    const char *name;
    int kind;
    int level;
    bool external;              /* declared in a block, but extern */
    struct entry *bnext;        /* in the bucket */
    struct entry *snext;        /* in the scope */
};

struct scope {
    struct entry *entries;
    struct scope *up;
};

struct parser {
    struct token *t;
    const struct cparse_hooks *h;
    struct entry *bucket[NBUCKET];
    struct scope *scope;
    int level;
    const char *func;
};

static unsigned hashname(const char *s)
{
    unsigned h = 2166136261u;
    while (*s)
        h = (h ^ (unsigned char)*s++) * 16777619u;
    return h % NBUCKET;
}

static void push_scope(struct parser *P)
{
    struct scope *s = xcalloc(1, sizeof *s);
    s->up = P->scope;
    P->scope = s;
    P->level++;
}

static void pop_scope(struct parser *P)
{
    struct scope *s = P->scope;
    while (s->entries) {
        struct entry *e = s->entries;
        s->entries = e->snext;
        struct entry **b = &P->bucket[hashname(e->name)];
        while (*b && *b != e)
            b = &(*b)->bnext;
        if (*b)
            *b = e->bnext;
        free(e);
    }
    P->scope = s->up;
    free(s);
    P->level--;
}

static struct entry *declare(struct parser *P, const char *name, int kind)
{
    struct entry *e = xcalloc(1, sizeof *e);
    e->name = name;
    e->kind = kind;
    e->level = P->level;
    unsigned h = hashname(name);
    e->bnext = P->bucket[h];
    P->bucket[h] = e;
    e->snext = P->scope->entries;
    P->scope->entries = e;
    return e;
}

static struct entry *lookup(struct parser *P, const char *name)
{
    for (struct entry *e = P->bucket[hashname(name)]; e; e = e->bnext)
        if (strcmp(e->name, name) == 0)
            return e;
    return NULL;
}

/* ---- words ---- */

static bool in(const char *s, const char *const *list)
{
    for (; *list; list++)
        if (strcmp(s, *list) == 0)
            return true;
    return false;
}

static const char *const storage_words[] = {
    "typedef", "extern", "static", "auto", "register", "_Thread_local", "thread_local",
    "__thread", "constexpr", NULL,
};
static const char *const qual_words[] = {
    "const", "volatile", "restrict", "__restrict", "__restrict__", "__const", "__const__",
    "__volatile", "__volatile__", "inline", "__inline", "__inline__", "_Noreturn",
    "__extension__", "_Atomic", NULL,
};
static const char *const type_words[] = {
    "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "_Bool",
    "bool", "_Complex", "__complex__", "_Imaginary", "__int128", "_Float16", "_Float32",
    "_Float64", "_Float128", "_Float32x", "_Float64x", "_Float128x", "_Decimal32",
    "_Decimal64", "_Decimal128", "__signed__", "__signed", "__builtin_va_list", "_BitInt",
    NULL,
};
static const char *const attr_words[] = {
    "__attribute__", "__attribute", "__declspec", "_Alignas", "alignas", "__asm__", "__asm",
    "asm", NULL,
};
static const char *const typeof_words[] = {
    "typeof", "__typeof__", "__typeof", "typeof_unqual", "__typeof_unqual__", NULL,
};
static const char *const stmt_words[] = {
    "if", "else", "while", "do", "for", "switch", "case", "default", "return", "goto",
    "break", "continue", "sizeof", "_Alignof", "alignof", "__alignof__", "__alignof",
    "_Generic", "_Static_assert", "static_assert", "struct", "union", "enum", "nullptr",
    "true", "false", "__builtin_offsetof", "__builtin_va_arg", NULL,
};

static bool is_keyword(const char *s)
{
    return in(s, storage_words) || in(s, qual_words) || in(s, type_words) || in(s, attr_words) ||
           in(s, typeof_words) || in(s, stmt_words);
}

static bool is(const struct token *t, const char *s)
{
    return tok_is(t, s);
}

static bool is_ident(const struct token *t)
{
    return t->kind == CPP_IDENT;
}

static void next(struct parser *P)
{
    if (P->t->kind != CPP_EOF)
        P->t = P->t->next;
}

/* skip a balanced (), [] or {} group starting at the opening token */
static void skip_group(struct parser *P)
{
    int level = 0;
    while (P->t->kind != CPP_EOF) {
        if (is(P->t, "(") || is(P->t, "[") || is(P->t, "{"))
            level++;
        else if (is(P->t, ")") || is(P->t, "]") || is(P->t, "}"))
            level--;
        next(P);
        if (level <= 0)
            return;
    }
}

/* attributes: __attribute__((...)), __declspec(...), _Alignas(...),
   asm("label"), [[...]] */
static bool skip_attributes(struct parser *P)
{
    bool any = false;
    for (;;) {
        if (is_ident(P->t) && in(P->t->text, attr_words)) {
            next(P);
            while (is_ident(P->t) && (is(P->t, "volatile") || is(P->t, "__volatile__") || is(P->t, "goto")))
                next(P);
            if (is(P->t, "("))
                skip_group(P);
            any = true;
        } else if (is(P->t, "[") && is(P->t->next, "[")) {
            skip_group(P);
            any = true;
        } else {
            return any;
        }
    }
}

static bool is_typedef_name(struct parser *P, const struct token *t)
{
    if (!is_ident(t) || is_keyword(t->text))
        return false;
    struct entry *e = lookup(P, t->text);
    return e && e->kind == CD_TYPEDEF;
}

/* does t start a type name (a cast, sizeof, a parameter)? */
static bool type_start(struct parser *P, const struct token *t)
{
    if (!is_ident(t))
        return false;
    const char *s = t->text;
    if (in(s, type_words) || in(s, qual_words) || in(s, typeof_words) || strcmp(s, "struct") == 0 ||
        strcmp(s, "union") == 0 || strcmp(s, "enum") == 0)
        return true;
    return is_typedef_name(P, t);
}

/* an undeclared name used as a type (in a file not preprocessed) */
static bool guess_type(struct parser *P, const struct token *t)
{
    if (!is_ident(t) || is_keyword(t->text) || lookup(P, t->text))
        return false;
    const struct token *n = t->next;
    if (is_ident(n) && !in(n->text, stmt_words) && !in(n->text, attr_words))
        return true;
    if (P->level == 0 && (is(n, "*")))
        return true;
    return false;
}

/* does t start a declaration? */
static bool decl_start(struct parser *P, const struct token *t)
{
    if (!is_ident(t))
        return is(t, "[") && is(t->next, "[");
    const char *s = t->text;
    if (in(s, storage_words) || in(s, attr_words) || type_start(P, t))
        return !(is_typedef_name(P, t) && is(t->next, ":"));
    return guess_type(P, t);
}

/* ---- text ---- */

struct sbuf {
    char *s;
    size_t n, cap;
};

static void sb_add(struct sbuf *b, const char *s)
{
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        b->cap = (b->n + l + 1) * 2;
        b->s = xrealloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, s, l + 1);
    b->n += l;
}

static void sb_word(struct sbuf *b, const char *s)
{
    if (b->n)
        sb_add(b, " ");
    sb_add(b, s);
}

static const char *sb_str(struct sbuf *b)
{
    if (!b->s)
        sb_add(b, "");
    return b->s;
}

/* ---- hooks ---- */

static void emit_ref(struct parser *P, const struct token *t, bool call, bool assign)
{
    if (!P->h->ref)
        return;
    struct entry *e = lookup(P, t->text);
    struct cref r = { t, P->func, call, assign, e ? e->kind : -1, e && e->level > 0 && !e->external };
    P->h->ref(P->h->arg, &r);
}

static void emit_decl(struct parser *P, struct cdecl *d)
{
    d->func = P->func;
    d->file_scope = P->level == 0;
    if (P->h->decl)
        P->h->decl(P->h->arg, d);
}

/* ---- expressions ---- */

enum { S_COMMA = 1, S_RPAREN = 2, S_RBRACKET = 4, S_COLON = 8 };

static void parse_compound(struct parser *P, const struct token **end);
static void parse_typename(struct parser *P);

static bool assign_op(const struct token *t)
{
    static const char *const ops[] = {
        "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "++", "--", NULL,
    };
    return t->kind == CPP_PUNCT && in(t->text, ops);
}

/* is the object named by t assigned to: t, then subscripts and members
   (".", not "->": those are of another object), then =, op=, ++, --? */
static bool assigned(const struct token *t)
{
    const struct token *u = t->next;
    for (;;) {
        if (is(u, "[")) {
            int level = 0;
            for (; u->kind != CPP_EOF; u = u->next) {
                if (is(u, "["))
                    level++;
                else if (is(u, "]") && --level == 0)
                    break;
                else if (is(u, ";"))
                    return false;
            }
            if (u->kind == CPP_EOF)
                return false;
            u = u->next;
        } else if (is(u, ".") && is_ident(u->next)) {
            u = u->next->next;
        } else {
            break;
        }
    }
    return assign_op(u);
}

/* scan an expression (or an initializer) to a stop token at its level,
   a ";", or an unbalanced "}" */
static void scan_expr(struct parser *P, int stops)
{
    int level = 0;
    const struct token *prev = NULL;
    while (P->t->kind != CPP_EOF) {
        struct token *t = P->t;
        if (is(t, ";"))
            return;
        if (level == 0) {
            if (((stops & S_COMMA) && is(t, ",")) || ((stops & S_RPAREN) && is(t, ")")) ||
                ((stops & S_RBRACKET) && is(t, "]")) || ((stops & S_COLON) && is(t, ":")) || is(t, "}") ||
                is(t, ")") || is(t, "]"))
                return;
        }
        if (is(t, "(") && is(t->next, "{")) {
            /* a statement expression */
            next(P);
            parse_compound(P, NULL);
            if (is(P->t, ")"))
                next(P);
            prev = NULL;
            continue;
        }
        if (is(t, "(") && type_start(P, t->next)) {
            /* a cast or a compound literal */
            next(P);
            parse_typename(P);
            if (is(P->t, ")"))
                next(P);
            prev = NULL;
            continue;
        }
        if (is(t, "(") || is(t, "[") || is(t, "{"))
            level++;
        else if (is(t, ")") || is(t, "]") || is(t, "}"))
            level--;
        if (is_ident(t)) {
            const char *s = t->text;
            if (strcmp(s, "sizeof") == 0 || strcmp(s, "_Alignof") == 0 || strcmp(s, "alignof") == 0 ||
                strcmp(s, "__alignof__") == 0 || strcmp(s, "__alignof") == 0) {
                next(P);
                if (is(P->t, "(") && type_start(P, P->t->next)) {
                    next(P);
                    parse_typename(P);
                    if (is(P->t, ")"))
                        next(P);
                }
                prev = NULL;
                continue;
            }
            if (prev && (is(prev, ".") || is(prev, "->"))) {
                /* a member */
            } else if (is_typedef_name(P, t)) {
                emit_ref(P, t, false, false);
            } else if (!is_keyword(s)) {
                bool assign = assigned(t) || (prev && (is(prev, "++") || is(prev, "--")));
                emit_ref(P, t, is(t->next, "("), assign);
            }
        }
        prev = t;
        next(P);
    }
}

/* ---- declarations ---- */

struct spec {
    bool is_typedef, is_extern, is_static, saw_type, any;
    struct sbuf type;
};

struct param {
    const struct token *name;
    char *type;
    bool knr;                   /* a name of an identifier list */
};

struct params {
    struct param *v;
    size_t n;
};

struct dres {
    const struct token *name;
    struct params *params;      /* of the function the name is */
};

static bool parse_specifiers(struct parser *P, struct spec *sp);
static char declarator(struct parser *P, struct dres *r, struct sbuf *out);

static void parse_struct_body(struct parser *P)
{
    next(P);                    /* { */
    while (P->t->kind != CPP_EOF && !is(P->t, "}")) {
        struct token *start = P->t;
        if (is(P->t, ";")) {
            next(P);
            continue;
        }
        if (is(P->t, "_Static_assert") || is(P->t, "static_assert")) {
            next(P);
            if (is(P->t, "("))
                skip_group(P);
            continue;
        }
        struct spec sp = { 0 };
        parse_specifiers(P, &sp);
        while (P->t->kind != CPP_EOF && !is(P->t, ";") && !is(P->t, "}")) {
            struct dres r = { 0 };
            struct sbuf b = { 0 };
            if (!is(P->t, ":"))
                declarator(P, &r, &b);
            free(b.s);
            skip_attributes(P);
            if (is(P->t, ":")) {
                next(P);
                scan_expr(P, S_COMMA);
            }
            skip_attributes(P);
            if (is(P->t, ","))
                next(P);
            else
                break;
        }
        free(sp.type.s);
        while (P->t->kind != CPP_EOF && !is(P->t, ";") && !is(P->t, "}")) {
            if (is(P->t, "(") || is(P->t, "[") || is(P->t, "{"))
                skip_group(P);
            else
                next(P);
        }
        if (is(P->t, ";"))
            next(P);
        if (P->t == start)
            next(P);
    }
    if (is(P->t, "}"))
        next(P);
}

static void parse_enum_body(struct parser *P)
{
    next(P);                    /* { */
    while (P->t->kind != CPP_EOF && !is(P->t, "}")) {
        struct token *start = P->t;
        if (is_ident(P->t)) {
            struct cdecl d = { 0 };
            d.kind = CD_ENUMCONST;
            d.name = P->t;
            d.type = "int";
            emit_decl(P, &d);
            declare(P, P->t->text, CD_ENUMCONST);
            next(P);
            skip_attributes(P);
            if (is(P->t, "=")) {
                next(P);
                scan_expr(P, S_COMMA);
            }
        }
        if (is(P->t, ","))
            next(P);
        else if (P->t == start)
            next(P);
    }
    if (is(P->t, "}"))
        next(P);
}

static bool parse_specifiers(struct parser *P, struct spec *sp)
{
    for (;;) {
        struct token *t = P->t;
        if (skip_attributes(P)) {
            sp->any = true;
            continue;
        }
        if (!is_ident(t))
            break;
        const char *s = t->text;
        if (in(s, storage_words)) {
            if (strcmp(s, "typedef") == 0)
                sp->is_typedef = true;
            else if (strcmp(s, "extern") == 0)
                sp->is_extern = true;
            else if (strcmp(s, "static") == 0)
                sp->is_static = true;
            sp->any = true;
            next(P);
            if (strcmp(s, "extern") == 0 && P->t->kind == CPP_STR)
                next(P);        /* extern "C" */
            continue;
        }
        if (strcmp(s, "_Atomic") == 0 && is(t->next, "(")) {
            next(P);
            next(P);
            parse_typename(P);
            if (is(P->t, ")"))
                next(P);
            sb_word(&sp->type, "_Atomic()");
            sp->saw_type = sp->any = true;
            continue;
        }
        if (in(s, qual_words)) {
            sp->any = true;
            next(P);
            continue;
        }
        if (in(s, type_words)) {
            sb_word(&sp->type, s);
            sp->saw_type = sp->any = true;
            next(P);
            if (strcmp(s, "_BitInt") == 0 && is(P->t, "("))
                skip_group(P);
            continue;
        }
        if (in(s, typeof_words)) {
            next(P);
            if (is(P->t, "("))
                skip_group(P);
            sb_word(&sp->type, "typeof()");
            sp->saw_type = sp->any = true;
            continue;
        }
        if (strcmp(s, "struct") == 0 || strcmp(s, "union") == 0 || strcmp(s, "enum") == 0) {
            next(P);
            skip_attributes(P);
            const struct token *tag = NULL;
            if (is_ident(P->t) && !is_keyword(P->t->text)) {
                tag = P->t;
                next(P);
            }
            skip_attributes(P);
            if (s[0] == 'e' && is(P->t, ":")) {        /* C23: enum E : type */
                next(P);
                struct spec u = { 0 };
                parse_specifiers(P, &u);
                free(u.type.s);
            }
            bool body = is(P->t, "{");
            if (tag) {
                struct cdecl d = { 0 };
                d.kind = CD_TAG;
                d.name = tag;
                d.tagkind = s;
                d.has_body = body;
                d.type = s;
                emit_decl(P, &d);
            }
            if (body) {
                if (s[0] == 'e')
                    parse_enum_body(P);
                else
                    parse_struct_body(P);
            }
            sb_word(&sp->type, s);
            if (tag)
                sb_word(&sp->type, tag->text);
            sp->saw_type = sp->any = true;
            continue;
        }
        if (!sp->saw_type && (is_typedef_name(P, t) || guess_type(P, t))) {
            if (is_typedef_name(P, t))
                emit_ref(P, t, false, false);
            sb_word(&sp->type, s);
            sp->saw_type = sp->any = true;
            next(P);
            continue;
        }
        break;
    }
    return sp->any;
}

/* is a "(" at t the start of a nested declarator (not of parameters)? */
static bool nested_declarator(struct parser *P, const struct token *t)
{
    const struct token *n = t->next;
    if (is(n, "*") || is(n, "^") || is(n, "(") || (is(n, "[") && is(n->next, "[")))
        return true;
    if (is_ident(n))
        return !type_start(P, n) && !in(n->text, storage_words) && !in(n->text, attr_words) &&
               !guess_type(P, n);
    return false;
}

static struct params *parse_params(struct parser *P)
{
    struct params *ps = xcalloc(1, sizeof *ps);
    next(P);                    /* ( */
    while (P->t->kind != CPP_EOF && !is(P->t, ")")) {
        struct token *start = P->t;
        if (is(P->t, "...")) {
            next(P);
            continue;
        }
        if (is(P->t, ",")) {
            next(P);
            continue;
        }
        struct spec sp = { 0 };
        struct param pm = { 0 };
        if (is_ident(P->t) && !is_keyword(P->t->text) && !is_typedef_name(P, P->t) &&
            (is(P->t->next, ",") || is(P->t->next, ")"))) {
            pm.name = P->t;
            pm.type = xstrdup("int");
            pm.knr = true;
            next(P);
        } else if (parse_specifiers(P, &sp)) {
            struct dres r = { 0 };
            struct sbuf b = { 0 };
            declarator(P, &r, &b);
            skip_attributes(P);
            pm.name = r.name;
            struct sbuf ty = { 0 };
            sb_add(&ty, sb_str(&sp.type));
            const char *a = sb_str(&b);
            if (a[0] == '*' || (a[0] == '(' && a[1] != ')'))
                sb_add(&ty, " ");
            sb_add(&ty, a);
            pm.type = (char *)sb_str(&ty);
            free(b.s);
            free(sp.type.s);
        } else {
            /* not a parameter: skip it */
            while (P->t->kind != CPP_EOF && !is(P->t, ",") && !is(P->t, ")") && !is(P->t, ";") &&
                   !is(P->t, "{") && !is(P->t, "}")) {
                if (is(P->t, "(") || is(P->t, "["))
                    skip_group(P);
                else
                    next(P);
            }
            if (is(P->t, ";") || is(P->t, "{") || is(P->t, "}"))
                break;
        }
        if (pm.type || pm.name) {
            ps->v = xrealloc(ps->v, (ps->n + 1) * sizeof *ps->v);
            ps->v[ps->n++] = pm;
        }
        if (P->t == start)
            next(P);
    }
    if (is(P->t, ")"))
        next(P);
    return ps;
}

static void free_params(struct params *ps)
{
    if (!ps)
        return;
    for (size_t i = 0; i < ps->n; i++)
        free(ps->v[i].type);
    free(ps->v);
    free(ps);
}

/* a declarator: its name (if any) in r, its abstract form in out; the
   result: what the name is: 'f' a function, 'p' a pointer, 'a' an array,
   'n' none of these */
static char declarator(struct parser *P, struct dres *r, struct sbuf *out)
{
    int nptr = 0;
    skip_attributes(P);
    while (is(P->t, "*") || is(P->t, "^")) {
        nptr++;
        next(P);
        while (is_ident(P->t) && in(P->t->text, qual_words))
            next(P);
        skip_attributes(P);
    }
    struct sbuf inner = { 0 };
    char derive = 'n';
    bool here = false;          /* the name is at this level */
    bool nested = false;
    if (is(P->t, "(") && nested_declarator(P, P->t)) {
        next(P);
        derive = declarator(P, r, &inner);
        if (is(P->t, ")"))
            next(P);
        nested = true;
    } else if (is_ident(P->t) && !is_keyword(P->t->text)) {
        if (!r->name)
            r->name = P->t;
        here = true;
        next(P);
    }
    skip_attributes(P);
    struct sbuf suf = { 0 };
    char first = 0;
    struct params *fparams = NULL;
    for (;;) {
        if (is(P->t, "[")) {
            next(P);
            scan_expr(P, S_RBRACKET);
            if (is(P->t, "]"))
                next(P);
            sb_add(&suf, "[]");
            if (!first)
                first = 'a';
        } else if (is(P->t, "(")) {
            struct params *ps = parse_params(P);
            sb_add(&suf, "()");
            if (!first) {
                first = 'f';
                fparams = ps;
            } else {
                free_params(ps);
            }
        } else {
            break;
        }
        skip_attributes(P);
    }
    for (int i = 0; i < nptr; i++)
        sb_add(out, "*");
    if (nested) {
        sb_add(out, "(");
        sb_add(out, sb_str(&inner));
        sb_add(out, ")");
    }
    if (suf.s)
        sb_add(out, suf.s);
    free(inner.s);
    free(suf.s);
    char result;
    if (nested && derive != 'n')
        result = derive;
    else if (here || nested)
        result = first ? first : nptr ? 'p' : 'n';
    else
        result = first ? first : nptr ? 'p' : 'n';
    if (result == 'f' && fparams && (here || (nested && derive == 'n')) && !r->params)
        r->params = fparams;
    else
        free_params(fparams);
    return result;
}

static void parse_typename(struct parser *P)
{
    struct spec sp = { 0 };
    parse_specifiers(P, &sp);
    struct dres r = { 0 };
    struct sbuf b = { 0 };
    declarator(P, &r, &b);
    free_params(r.params);
    free(b.s);
    free(sp.type.s);
}

static char *compose(const char *type, const char *abs)
{
    struct sbuf b = { 0 };
    sb_add(&b, type[0] ? type : "int");
    if (abs[0] == '*' || (abs[0] == '(' && abs[1] != ')'))
        sb_add(&b, " ");
    sb_add(&b, abs);
    return (char *)sb_str(&b);
}

/* is this a K&R function definition: parameter declarations, then "{"? */
static bool knr_definition(struct parser *P, const struct params *ps)
{
    if (!ps || ps->n == 0)
        return false;
    for (size_t i = 0; i < ps->n; i++)
        if (!ps->v[i].knr)
            return false;
    if (!decl_start(P, P->t) && !(is_ident(P->t) && !is_keyword(P->t->text)))
        return false;
    bool semi = false;
    for (const struct token *t = P->t; t->kind != CPP_EOF; t = t->next) {
        if (is(t, ";"))
            semi = true;
        else if (is(t, "{"))
            return semi;
        else if (is(t, "}"))
            return false;
    }
    return false;
}

/* skip to the end of a declaration that does not parse */
static void recover(struct parser *P)
{
    while (P->t->kind != CPP_EOF && !is(P->t, ";") && !is(P->t, "}")) {
        if (is(P->t, "(") || is(P->t, "[") || is(P->t, "{"))
            skip_group(P);
        else
            next(P);
    }
    if (is(P->t, ";"))
        next(P);
}

static void parse_declaration(struct parser *P, bool knr_params);

static void function_definition(struct parser *P, const struct token *name, const char *type,
                                const struct spec *sp, struct params *ps)
{
    struct entry *e = lookup(P, name->text);
    if (!e || e->level != P->level)
        declare(P, name->text, CD_FUNC);
    const char *outer = P->func;
    P->func = name->text;
    push_scope(P);
    bool knr = ps && ps->n > 0 && ps->v[0].knr;
    for (size_t i = 0; ps && i < ps->n; i++) {
        if (!ps->v[i].name)
            continue;
        if (knr) {
            /* declared in the declarations that follow, or int */
            bool found = false;
            for (const struct token *t = P->t; t->kind != CPP_EOF && !is(t, "{"); t = t->next)
                if (is_ident(t) && strcmp(t->text, ps->v[i].name->text) == 0)
                    found = true;
            if (found)
                continue;
        }
        struct cdecl d = { 0 };
        d.kind = CD_PARAM;
        d.name = ps->v[i].name;
        d.type = ps->v[i].type;
        emit_decl(P, &d);
        declare(P, ps->v[i].name->text, CD_VAR);
    }
    while (P->t->kind != CPP_EOF && !is(P->t, "{")) {
        struct token *start = P->t;
        parse_declaration(P, true);
        if (P->t == start)
            next(P);
    }
    const struct token *end = NULL;
    parse_compound(P, &end);
    pop_scope(P);
    P->func = outer;
    struct cdecl d = { 0 };
    d.kind = CD_FUNCDEF;
    d.name = name;
    d.type = type;
    d.is_static = sp->is_static;
    d.is_extern = sp->is_extern;
    d.end = end;
    emit_decl(P, &d);
}

static void parse_declaration(struct parser *P, bool knr_params)
{
    struct spec sp = { 0 };
    bool any = parse_specifiers(P, &sp);
    if (!any) {
        /* int is implied: f() { ... } (at file scope) */
        if (!(P->level == 0 && is_ident(P->t) && !is_keyword(P->t->text) && is(P->t->next, "("))) {
            free(sp.type.s);
            return;
        }
    }
    if (is(P->t, ";")) {
        next(P);
        free(sp.type.s);
        return;
    }
    for (;;) {
        struct dres r = { 0 };
        struct sbuf abs = { 0 };
        char derive = declarator(P, &r, &abs);
        skip_attributes(P);
        if (!r.name) {
            free(abs.s);
            free_params(r.params);
            recover(P);
            break;
        }
        char *type = compose(sb_str(&sp.type), sb_str(&abs));
        free(abs.s);
        if (derive == 'f' && !sp.is_typedef && !knr_params &&
            (is(P->t, "{") || knr_definition(P, r.params))) {
            function_definition(P, r.name, type, &sp, r.params);
            free_params(r.params);
            /* the type stays: the hooks may keep it */
            break;
        }
        free_params(r.params);
        struct cdecl d = { 0 };
        d.kind = knr_params ? CD_PARAM : sp.is_typedef ? CD_TYPEDEF : derive == 'f' ? CD_FUNC : CD_VAR;
        d.name = r.name;
        d.type = type;
        d.is_extern = sp.is_extern;
        d.is_static = sp.is_static;
        d.has_init = is(P->t, "=");
        int kind = d.kind == CD_PARAM ? CD_VAR : d.kind;
        struct entry *e = lookup(P, r.name->text);
        if (!e || e->level != P->level || e->kind != kind)
            e = declare(P, r.name->text, kind);
        if (P->level > 0 && (sp.is_extern || kind == CD_FUNC) && d.kind != CD_PARAM)
            e->external = true;
        emit_decl(P, &d);
        if (is(P->t, "=")) {
            next(P);
            scan_expr(P, S_COMMA);
        }
        if (is(P->t, ",")) {
            next(P);
            continue;
        }
        if (is(P->t, ";"))
            next(P);
        else
            recover(P);
        break;
    }
    free(sp.type.s);
}

/* ---- statements ---- */

static void parse_statement(struct parser *P);

static void expect(struct parser *P, const char *s)
{
    if (is(P->t, s))
        next(P);
}

static void paren_expr(struct parser *P)
{
    if (is(P->t, "(")) {
        next(P);
        scan_expr(P, S_RPAREN);
        expect(P, ")");
    }
}

static void parse_block_item(struct parser *P)
{
    struct token *t = P->t;
    if (is_ident(t) && !is_keyword(t->text) && is(t->next, ":") && !is_typedef_name(P, t)) {
        next(P);                /* a label */
        next(P);
        return;
    }
    if (decl_start(P, t)) {
        parse_declaration(P, false);
        return;
    }
    parse_statement(P);
}

static void parse_statement(struct parser *P)
{
    struct token *t = P->t;
    if (is(t, "{")) {
        parse_compound(P, NULL);
        return;
    }
    if (is(t, ";")) {
        next(P);
        return;
    }
    if (is_ident(t)) {
        const char *s = t->text;
        if (!strcmp(s, "if") || !strcmp(s, "switch") || !strcmp(s, "while")) {
            next(P);
            paren_expr(P);
            parse_statement(P);
            if (!strcmp(s, "if") && is(P->t, "else")) {
                next(P);
                parse_statement(P);
            }
            return;
        }
        if (!strcmp(s, "for")) {
            next(P);
            push_scope(P);
            if (is(P->t, "(")) {
                next(P);
                if (decl_start(P, P->t))
                    parse_declaration(P, false);
                else {
                    scan_expr(P, 0);
                    expect(P, ";");
                }
                scan_expr(P, 0);
                expect(P, ";");
                scan_expr(P, S_RPAREN);
                expect(P, ")");
            }
            parse_statement(P);
            pop_scope(P);
            return;
        }
        if (!strcmp(s, "do")) {
            next(P);
            parse_statement(P);
            expect(P, "while");
            paren_expr(P);
            expect(P, ";");
            return;
        }
        if (!strcmp(s, "case")) {
            next(P);
            scan_expr(P, S_COLON);
            expect(P, ":");
            return;
        }
        if (!strcmp(s, "default")) {
            next(P);
            expect(P, ":");
            return;
        }
        if (!strcmp(s, "goto")) {
            next(P);
            if (is_ident(P->t))
                next(P);
            else
                scan_expr(P, 0);
            expect(P, ";");
            return;
        }
        if (!strcmp(s, "break") || !strcmp(s, "continue") || !strcmp(s, "else")) {
            next(P);
            expect(P, ";");
            return;
        }
        if (!strcmp(s, "return")) {
            next(P);
            scan_expr(P, 0);
            expect(P, ";");
            return;
        }
        if (in(s, attr_words) && strcmp(s, "_Alignas") != 0) {
            /* asm (...) */
            next(P);
            while (is_ident(P->t) && in(P->t->text, qual_words))
                next(P);
            if (is(P->t, "goto"))
                next(P);
            if (is(P->t, "("))
                skip_group(P);
            expect(P, ";");
            return;
        }
        if (!strcmp(s, "_Static_assert") || !strcmp(s, "static_assert")) {
            next(P);
            if (is(P->t, "("))
                skip_group(P);
            expect(P, ";");
            return;
        }
    }
    scan_expr(P, 0);
    if (is(P->t, ";") || is(P->t, ")") || is(P->t, "]"))
        next(P);
}

static void parse_compound(struct parser *P, const struct token **end)
{
    if (!is(P->t, "{"))
        return;
    next(P);
    push_scope(P);
    while (P->t->kind != CPP_EOF && !is(P->t, "}")) {
        struct token *start = P->t;
        parse_block_item(P);
        if (P->t == start)
            next(P);
    }
    if (end)
        *end = P->t;
    if (is(P->t, "}"))
        next(P);
    pop_scope(P);
}

/* ---- the translation unit ---- */

void cparse(struct token *t, const struct cparse_hooks *h)
{
    struct parser *P = xcalloc(1, sizeof *P);
    P->t = t;
    P->h = h;
    push_scope(P);
    P->level = 0;
    while (P->t->kind != CPP_EOF) {
        struct token *start = P->t;
        if (is(P->t, ";") || is(P->t, "}")) {
            next(P);
            continue;
        }
        if (is(P->t, "{")) {
            /* a block that is not a function's body (after a macro) */
            skip_group(P);
            continue;
        }
        if (is(P->t, "_Static_assert") || is(P->t, "static_assert")) {
            next(P);
            if (is(P->t, "("))
                skip_group(P);
            expect(P, ";");
            continue;
        }
        if (is(P->t, "asm") || is(P->t, "__asm__") || is(P->t, "__asm")) {
            next(P);
            recover(P);
            continue;
        }
        if (is(P->t, "extern") && P->t->next->kind == CPP_STR) {
            next(P);
            next(P);
            if (is(P->t, "{"))
                next(P);        /* extern "C" { ... } */
            continue;
        }
        parse_declaration(P, false);
        if (P->t == start) {
            /* not a declaration: skip a statement's worth */
            if (is(P->t, "(") || is(P->t, "["))
                skip_group(P);
            else
                next(P);
        }
    }
    P->level = 1;
    pop_scope(P);
    free(P);
}
