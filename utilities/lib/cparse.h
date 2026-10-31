/*
 * cparse.h - a forgiving parser of C declarations and references, for
 * cflow, ctags and cxref
 */

#ifndef CPARSE_H
#define CPARSE_H

#include "cpp.h"

#include <stdbool.h>

enum {
    CD_FUNCDEF,                 /* a function definition */
    CD_FUNC,                    /* a function declaration */
    CD_VAR,                     /* an object */
    CD_TYPEDEF,                 /* a typedef name */
    CD_PARAM,                   /* a parameter of a function definition */
    CD_ENUMCONST,               /* an enumeration constant */
    CD_TAG,                     /* a structure, union or enumeration tag */
};

struct cdecl {
    int kind;
    const struct token *name;
    bool file_scope;
    bool is_extern, is_static;
    bool has_init;              /* CD_VAR: with an initializer */
    const char *type;           /* the type, abstract: "int()", "char *" */
    const char *func;           /* the function it is in; NULL outside */
    const struct token *end;    /* CD_FUNCDEF: the "}" of the body */
    const char *tagkind;        /* CD_TAG: "struct", "union" or "enum" */
    bool has_body;              /* CD_TAG: with a member list */
};

struct cref {
    const struct token *name;
    const char *func;           /* the function it is in; NULL outside */
    bool call;                  /* followed by "(" */
    bool assign;                /* assigned to: =, op=, ++, -- (after any
                                   subscripts and "." members) */
    int kind;                   /* what the name is declared as (-1: not) */
    bool local;                 /* declared in a block (or a parameter) */
};

struct cparse_hooks {
    void (*decl)(void *arg, const struct cdecl *d);
    void (*ref)(void *arg, const struct cref *r);
    void *arg;
};

/* parse a token list (ending with CPP_EOF): preprocessed, or (for ctags)
   not, with the directives taken out */
void cparse(struct token *t, const struct cparse_hooks *h);

#endif
