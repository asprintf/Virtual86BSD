/*
 * Copyright (c) 2026, asprintf
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
 * mnttab.c - see mnttab.h
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "mnttab.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* undo the octal escapes in place */
static char *unescape(char *s)
{
    char *w = s;
    for (char *r = s; *r; ) {
        if (r[0] == '\\' && r[1] >= '0' && r[1] <= '3' && r[2] >= '0' && r[2] <= '7'
            && r[3] >= '0' && r[3] <= '7') {
            *w++ = (char)((r[1] - '0') * 64 + (r[2] - '0') * 8 + (r[3] - '0'));
            r += 4;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return s;
}

static char *dup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    return p ? memcpy(p, s, n) : NULL;
}

void mnt_free(struct mnt *list, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        free(list[i].fsname);
        free(list[i].dir);
        free(list[i].type);
        free(list[i].opts);
    }
    free(list);
}

bool mnt_hasopt(const char *opts, const char *w)
{
    size_t l = strlen(w);
    for (const char *p = opts; *p; ) {
        size_t k = strcspn(p, ",");
        if (k == l && strncmp(p, w, l) == 0)
            return true;
        p += k + (p[k] == ',');
    }
    return false;
}

bool mnt_read(const char *const *names, struct mnt **list, size_t *n)
{
    FILE *f = NULL;
    int err = ENOENT;
    for (size_t i = 0; names[i] && !f; i++)
        if (!(f = fopen(names[i], "r")))
            err = errno;
    if (!f) {
        errno = err;
        return false;
    }
    struct mnt *l = NULL;
    size_t count = 0, cap = 0;
    char *line = NULL;
    size_t lcap = 0;
    while (getline(&line, &lcap, f) > 0) {
        char *save = NULL, *fld[4];
        int k = 0;
        for (char *t = strtok_r(line, " \t\n", &save); t && k < 4; t = strtok_r(NULL, " \t\n", &save))
            fld[k++] = t;
        if (k < 2 || fld[0][0] == '#')
            continue;
        if (count == cap) {
            size_t nc = cap ? 2 * cap : 16;
            struct mnt *nl = realloc(l, nc * sizeof *l);
            if (!nl)
                goto nomem;
            l = nl;
            cap = nc;
        }
        for (int j = k; j < 4; j++)
            fld[j] = (char *)"";
        struct mnt *m = &l[count];
        m->fsname = dup(unescape(fld[0]));
        m->dir = dup(unescape(fld[1]));
        m->type = dup(unescape(fld[2]));
        m->opts = dup(unescape(fld[3]));
        count++;
        if (!m->fsname || !m->dir || !m->type || !m->opts)
            goto nomem;
    }
    free(line);
    fclose(f);
    *list = l;
    *n = count;
    return true;
nomem:
    free(line);
    fclose(f);
    mnt_free(l, count);
    errno = ENOMEM;
    return false;
}
