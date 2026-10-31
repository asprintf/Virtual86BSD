/*
 * mo.h - messages objects (the files msgfmt makes): reading them and
 * finding a message in one
 */

#ifndef MO_H
#define MO_H

#include <stdbool.h>
#include <stddef.h>

#include "plural.h"

struct mo {
    unsigned char *data;
    size_t size;
    bool swap;                  /* the other byte order */
    unsigned long n;            /* the messages */
    unsigned long orig, trans;  /* the offsets of their tables */
    unsigned long nplurals;     /* 0: no plural forms */
    struct plural *plural;
    char charset[64];           /* "" when the header names none */
};

/* load the messages object at path: false (errno set, or EINVAL when it
   is not a messages object) */
bool mo_load(const char *path, struct mo *m);
void mo_free(struct mo *m);

/* the translation of msgid: its bytes (the plural forms separated by NUL
   bytes) and their number in *len, or NULL */
const char *mo_find(const struct mo *m, const char *msgid, size_t *len);

#endif
