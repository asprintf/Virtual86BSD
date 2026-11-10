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
 * plural.c - plural expressions (see plural.h)
 *
 * The expressions of C on unsigned long values of n and decimal
 * constants: ?: || && == != < > <= >= + - * / % ! and parentheses, with
 * the precedence and associativity of C.  The operators are those that
 * are found in the plural forms of messages objects; others (such as
 * the bitwise ones) are not taken.
 */

#include "plural.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum op { NUM, VAR, NOT, MUL, DIV, MOD, ADD, SUB, LT, GT, LE, GE, EQ, NE, AND, OR, COND };

struct plural {
    enum op op;
    unsigned long num;
    struct plural *a, *b, *c;
};

struct parser {
    const char *s;
    bool bad;
    int depth;
};

static void skip(struct parser *p)
{
    while (*p->s == ' ' || *p->s == '\t' || *p->s == '\n' || *p->s == '\r')
        p->s++;
}

static struct plural *node(enum op op, struct plural *a, struct plural *b, struct plural *c)
{
    struct plural *n = calloc(1, sizeof *n);
    if (!n) {
        plural_free(a);
        plural_free(b);
        plural_free(c);
        return NULL;
    }
    n->op = op;
    n->a = a;
    n->b = b;
    n->c = c;
    return n;
}

static struct plural *cond(struct parser *p);

static struct plural *primary(struct parser *p)
{
    skip(p);
    if (++p->depth > 200) {
        p->bad = true;
        return NULL;
    }
    struct plural *r = NULL;
    if (*p->s == '(') {
        p->s++;
        r = cond(p);
        skip(p);
        if (!r || *p->s != ')') {
            plural_free(r);
            p->bad = true;
            r = NULL;
        } else {
            p->s++;
        }
    } else if (*p->s == '!') {
        p->s++;
        struct plural *a = primary(p);
        r = a ? node(NOT, a, NULL, NULL) : NULL;
    } else if (*p->s == 'n' && !isalnum((unsigned char)p->s[1]) && p->s[1] != '_') {
        p->s++;
        r = node(VAR, NULL, NULL, NULL);
    } else if (isdigit((unsigned char)*p->s)) {
        unsigned long v = 0;
        while (isdigit((unsigned char)*p->s)) {
            v = v * 10 + (unsigned long)(*p->s - '0');
            p->s++;
        }
        r = node(NUM, NULL, NULL, NULL);
        if (r)
            r->num = v;
    } else {
        p->bad = true;
    }
    p->depth--;
    if (!r)
        p->bad = true;
    return r;
}

/* the binary operators by level: the tokens and their operations */
static const struct {
    const char *tok;
    enum op op;
    int level;
} ops[] = {
    { "||", OR, 1 }, { "&&", AND, 2 }, { "==", EQ, 3 }, { "!=", NE, 3 },
    { "<=", LE, 4 }, { ">=", GE, 4 }, { "<", LT, 4 }, { ">", GT, 4 },
    { "+", ADD, 5 }, { "-", SUB, 5 }, { "*", MUL, 6 }, { "/", DIV, 6 }, { "%", MOD, 6 },
};

static int match_op(struct parser *p, int level, enum op *op)
{
    skip(p);
    for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        size_t l = strlen(ops[i].tok);
        if (ops[i].level == level && strncmp(p->s, ops[i].tok, l) == 0) {
            /* "<" is not the start of "<=", "|" alone is not an operator */
            if (l == 1 && (p->s[1] == '=' && (ops[i].tok[0] == '<' || ops[i].tok[0] == '>')))
                continue;
            *op = ops[i].op;
            return (int)l;
        }
    }
    return 0;
}

static struct plural *binary(struct parser *p, int level)
{
    if (level > 6)
        return primary(p);
    struct plural *l = binary(p, level + 1);
    enum op op;
    int k;
    while (l && (k = match_op(p, level, &op)) > 0) {
        p->s += k;
        struct plural *r = binary(p, level + 1);
        if (!r) {
            plural_free(l);
            return NULL;
        }
        l = node(op, l, r, NULL);
    }
    return l;
}

static struct plural *cond(struct parser *p)
{
    if (++p->depth > 200) {
        p->bad = true;
        return NULL;
    }
    struct plural *c = binary(p, 1);
    skip(p);
    if (c && *p->s == '?') {
        p->s++;
        struct plural *a = cond(p);
        skip(p);
        if (!a || *p->s != ':') {
            plural_free(c);
            plural_free(a);
            p->bad = true;
            p->depth--;
            return NULL;
        }
        p->s++;
        struct plural *b = cond(p);
        if (!b) {
            plural_free(c);
            plural_free(a);
            p->depth--;
            return NULL;
        }
        c = node(COND, c, a, b);
    }
    p->depth--;
    return c;
}

struct plural *plural_parse(const char *s, const char **end)
{
    struct parser p = { s, false, 0 };
    struct plural *r = cond(&p);
    skip(&p);
    if (r && !p.bad && (*p.s == '\0' || *p.s == ';')) {
        if (end)
            *end = p.s;
        return r;
    }
    plural_free(r);
    return NULL;
}

bool plural_eval(const struct plural *p, unsigned long n, unsigned long *v)
{
    unsigned long a, b;
    switch (p->op) {
    case NUM: *v = p->num; return true;
    case VAR: *v = n; return true;
    case NOT:
        if (!plural_eval(p->a, n, &a))
            return false;
        *v = !a;
        return true;
    case COND:
        if (!plural_eval(p->a, n, &a))
            return false;
        return plural_eval(a ? p->b : p->c, n, v);
    case AND:
        if (!plural_eval(p->a, n, &a))
            return false;
        if (!a) {
            *v = 0;
            return true;
        }
        if (!plural_eval(p->b, n, &b))
            return false;
        *v = b != 0;
        return true;
    case OR:
        if (!plural_eval(p->a, n, &a))
            return false;
        if (a) {
            *v = 1;
            return true;
        }
        if (!plural_eval(p->b, n, &b))
            return false;
        *v = b != 0;
        return true;
    default:
        break;
    }
    if (!plural_eval(p->a, n, &a) || !plural_eval(p->b, n, &b))
        return false;
    switch (p->op) {
    case MUL: *v = a * b; break;
    case DIV: if (b == 0) return false; *v = a / b; break;
    case MOD: if (b == 0) return false; *v = a % b; break;
    case ADD: *v = a + b; break;
    case SUB: *v = a - b; break;
    case LT: *v = a < b; break;
    case GT: *v = a > b; break;
    case LE: *v = a <= b; break;
    case GE: *v = a >= b; break;
    case EQ: *v = a == b; break;
    case NE: *v = a != b; break;
    default: return false;
    }
    return true;
}

void plural_free(struct plural *p)
{
    if (!p)
        return;
    plural_free(p->a);
    plural_free(p->b);
    plural_free(p->c);
    free(p);
}

/* the value after "key=" in s, with blanks allowed around '=' */
static const char *find_key(const char *s, const char *key)
{
    size_t l = strlen(key);
    for (const char *t = s; (t = strstr(t, key)); t++) {
        if (t > s && (isalnum((unsigned char)t[-1]) || t[-1] == '_'))
            continue;
        const char *v = t + l;
        while (*v == ' ' || *v == '\t')
            v++;
        if (*v != '=')
            continue;
        v++;
        while (*v == ' ' || *v == '\t')
            v++;
        return v;
    }
    return NULL;
}

int plural_header(const char *header, unsigned long *count, struct plural **expr)
{
    const char *v = find_key(header, "nplurals");
    if (!v)
        return 0;
    if (!isdigit((unsigned char)*v))
        return -1;
    char *e;
    unsigned long c = strtoul(v, &e, 10);
    const char *x = find_key(e, "plural");
    if (c == 0 || !x)
        return -1;
    /* the expression: to a ';' or the end of its line */
    const char *nl = strchr(x, '\n');
    size_t len = nl ? (size_t)(nl - x) : strlen(x);
    char *copy = malloc(len + 1);
    if (!copy)
        return -1;
    memcpy(copy, x, len);
    copy[len] = '\0';
    const char *end;
    struct plural *p = plural_parse(copy, &end);
    free(copy);
    if (!p)
        return -1;
    *count = c;
    *expr = p;
    return 1;
}

bool header_charset(const char *header, char *buf, unsigned long n)
{
    const char *v = find_key(header, "charset");
    if (!v)
        return false;
    size_t l = 0;
    while (v[l] && !strchr(" \t\n\r;", v[l]))
        l++;
    if (l == 0 || l >= n)
        return false;
    memcpy(buf, v, l);
    buf[l] = '\0';
    return true;
}
