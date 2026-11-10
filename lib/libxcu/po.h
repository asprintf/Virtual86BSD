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
 * po.h - reading portable messages object source files, dot-po files
 * (shared by msgfmt and xgettext)
 */

#ifndef PO_H
#define PO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct po_str {
    char *s;                    /* escapes processed, NUL-terminated */
    size_t len;                 /* (it may hold NUL bytes) */
};

struct po_msg {
    const char *domain;         /* the domain of its section */
    struct po_str ctxt;         /* msgctxt (an extension), s NULL if none */
    struct po_str id, plural;   /* plural.s NULL if none */
    struct po_str *strs;        /* msgstr, or msgstr[0..nstrs) */
    int nstrs;
    bool indexed;               /* msgstr[index] */
    bool fuzzy;
    int cformat;                /* 1 c-format, -1 no-c-format, 0 neither */
    const char *file;
    long line;                  /* of its msgid */
};

/* read the dot-po file f (named name in messages): fn is called for each
   message (the header too) and its directive "domain" for each domain
   directive (msg NULL, domain set); the number of errors, each reported
   on the standard error as "PROG: NAME:LINE: what" */
typedef void po_fn(const struct po_msg *msg, const char *domain, void *arg);
int po_read(FILE *f, const char *name, const char *prog, po_fn *fn, void *arg);

#endif
