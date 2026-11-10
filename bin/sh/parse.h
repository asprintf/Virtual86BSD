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
 * parse.h - the interface between the shell grammar (sh.y) and the
 * token classifier in parser.c.
 */
#ifndef PARSE_H
#define PARSE_H

/* a token as the grammar sees it */
typedef struct Tok {
    int type;           /* the raw token (T_* in parser.c) */
    int ytok;           /* the token code given to the grammar */
    int line;
    Word *word;         /* WORD, ASSIGNMENT_WORD, FNAME and reserved words */
    int ionum;          /* IO_NUMBER */
    int word_derived;   /* a word, possibly recognized as a reserved word */
    int cmd_pos;        /* in a place where a command must begin */
    Redir *here;        /* the redirection whose here-document this word delimits */
} Tok;

/* growing lists for the grammar's actions */
typedef struct Seq { Node **v; int n, cap; } Seq;           /* pipeline */
typedef struct WList { Word *head, **tail; } WList;
typedef struct RList { Redir *head, **tail; } RList;
typedef struct CList { CaseItem *head, **tail; } CList;
typedef struct SCmd { Node *n; Word **at, **wt; Redir **rt; } SCmd;
typedef struct RV { Redir *r; int line; } RV;               /* a redirection and its first line */

/* syntax error contexts: what the parser expected */
enum { CTX_GENERIC, CTX_EXPECT, CTX_RPAREN, CTX_CMDSUB };
_Noreturn void shy_syntax(int ctx, const char *expected);

extern Node *parse_result;
extern int parse_at_eof;

Node *new_node(int type, int line);
Node *make_list(Node *a, Node *b, int line);
int node_first_line(Node *n);

int shylex(void);
void shyerror(const char *s);

#endif
