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
