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
 * parse.h - the interface between the scanner (awk.l), the grammar
 * (awk.y) and the symbol tables (parse.c).
 */
#ifndef PARSE_H
#define PARSE_H

/* a token: its TK_ code, where it starts in the program text (for the
   context in messages) and its value */
typedef struct Tok {
    int type;
    size_t start;
    int line;
    Str *str;           /* STRING, ERE, NAME, FUNC_NAME, BUILTIN */
    double num;         /* NUMBER */
    int bi;             /* BUILTIN: the function */
} Tok;

/* a list of nodes being built (linked by next) */
typedef struct List { Node *head, *last; int n; } List;

/* scanner */
extern Tok *lex_la;                 /* the token read last (the lookahead) */
void lex_begin(const char *src, size_t len);
void lex_reset(void);
int yylex(void);
_Noreturn void syntax_error_at(const Tok *t, const char *msg);
void lex_need_lookahead(void);      /* read the lookahead now if it is not read */
int lex_pipe_getline(void);         /* the token after the lookahead "|" is getline */

/* parse.c */
int word_token(const char *w, int *bi);   /* TK_ keyword, TK_BUILTIN or 0 */
void add_rule(Node *p1, Node *p2, Node *act);
void add_begin(Node *act);
void add_end(Node *act);

/* awk.y */
int yyparse(void);
void parser_reset(void);

#endif
