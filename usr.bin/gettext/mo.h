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
