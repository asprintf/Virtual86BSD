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
 * node.c - the table of targets and prerequisites, and of inference
 * rules.
 */
#include "make.h"

#define NHASH 4096
static Node *ntab[NHASH];
static Node wait_storage = { .name = ".WAIT" };
Node *wait_node = &wait_storage;

static unsigned hash(const char *s)
{
    unsigned h = 5381;
    while (*s)
        h = h * 33 + (unsigned char)*s++;
    return h % NHASH;
}

Node *node_find(const char *name)
{
    for (Node *n = ntab[hash(name)]; n; n = n->hnext)
        if (strcmp(n->name, name) == 0)
            return n;
    return NULL;
}

Node *node_get(const char *name)
{
    Node *n = node_find(name);
    unsigned h;
    if (n)
        return n;
    h = hash(name);
    n = xcalloc(1, sizeof *n);
    n->name = xstrdup(name);
    parse_lib_member(name, &n->lib, &n->member);
    n->hnext = ntab[h];
    ntab[h] = n;
    return n;
}

void node_add_pre(Node *n, Node *p)
{
    if (n->npre + 1 > n->cappre) {
        n->cappre = n->cappre ? n->cappre * 2 : 4;
        n->pre = xrealloc(n->pre, (size_t)n->cappre * sizeof(Node *));
    }
    n->pre[n->npre++] = p;
}

void node_foreach(void (*fn)(Node *n))
{
    for (int i = 0; i < NHASH; i++)
        for (Node *n = ntab[i]; n; n = n->hnext)
            fn(n);
}

static void reset(Node *n)
{
    n->state = ST_NEW;
    n->ok = 0;
    n->stat_done = 0;
    n->newer = 0;
    n->made = 0;
    n->visiting = 0;
    free(n->all);
    n->all = NULL;
    n->nall = 0;
    n->implicit = NULL;
    n->rcmds = NULL;
    free(n->stem);
    n->stem = NULL;
    n->from_default = 0;
    n->dflt_ar = 0;
}

void node_reset_all(void)
{
    node_foreach(reset);
}

/* ---------- inference rules ---------- */

static Infer *itab;

Infer *infer_get(const char *name)
{
    for (Infer *r = itab; r; r = r->next)
        if (strcmp(r->name, name) == 0)
            return r;
    return NULL;
}

void infer_set(const char *name, Cmds *c)
{
    Infer *r = infer_get(name);
    if (!r) {
        r = xcalloc(1, sizeof *r);
        r->name = xstrdup(name);
        r->next = itab;
        itab = r;
    }
    r->cmds = c;
}

void infer_clear_all(void)
{
    itab = NULL;
}

void infer_foreach(void (*fn)(Infer *r))
{
    for (Infer *r = itab; r; r = r->next)
        fn(r);
}
