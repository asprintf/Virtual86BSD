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
