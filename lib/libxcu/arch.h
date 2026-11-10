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
 * arch.h - archives of the format the link editor reads (that of GNU ar
 * and System V: "!<arch>\n", 60-byte member headers, the long names in
 * the member "//", the symbol table in the member "/"), read and written
 * (shared by ar, nm and strip)
 */

#ifndef ARCH_H
#define ARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct ar_member {
    char *name;                 /* without the trailing slash */
    long long mtime;
    long uid, gid;
    unsigned long mode;
    unsigned char *data;
    size_t size;
};

struct archive {
    struct ar_member *m;
    size_t n, cap;
    bool had_symtab;            /* it had a symbol table */
};

/* is data an archive?  (the magic string) */
bool ar_is(const unsigned char *data, size_t size);
/* read the archive in data (the members' data copied): false with *why
   set when it is not a valid archive */
bool ar_parse(struct archive *a, const unsigned char *data, size_t size, const char **why);
void ar_free(struct archive *a);

/* the archive as bytes (malloc'd), with a symbol table when one of the
   members is an object file this can read (or when force_symtab and
   there is one); symtab_time: the date in the symbol table's header */
unsigned char *ar_build(const struct archive *a, size_t *size, long long symtab_time);

/* read a whole file (malloc'd): NULL with errno set */
unsigned char *ar_slurp(const char *path, size_t *size);

#endif
