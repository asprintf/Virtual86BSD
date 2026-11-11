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
 * mnttab.h - a table of file systems: the mounts of the system
 * (/proc/self/mounts) or an fstab.  Both are lines of
 *     special  directory  type  options  [frequency  passno]
 * with the blanks of a name written \040 (\011 tab, \012 newline, \134
 * backslash).  Shared by df, mount and umount.
 */

#ifndef MNTTAB_H
#define MNTTAB_H

#include <stdbool.h>
#include <stddef.h>

struct mnt {
    char *fsname;               /* the special file, "/dev/disk0s1" */
    char *dir;                  /* where it is mounted */
    char *type;                 /* "ext2", "tmpfs" ... ("" in a table of two fields) */
    char *opts;                 /* "rw", "ro,noauto" ... ("" if none) */
};

/* read the table in the first of names (a NULL ends the list) that opens;
   *list is malloc'ed (free it with mnt_free).  Lines that are blank, start
   with # or have fewer than two fields are skipped; a missing third or
   fourth field is "".  False with errno set (the last open's) if none
   opens, or ENOMEM */
bool mnt_read(const char *const *names, struct mnt **list, size_t *n);
void mnt_free(struct mnt *list, size_t n);
/* the options (comma separated) have the word w */
bool mnt_hasopt(const char *opts, const char *w);

#endif
