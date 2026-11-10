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
 * utx.h - the user accounting database (utmpx), read through
 * getutxent() or from a file of struct utmpx records; and the terminals
 * of its lines (shared by who, write, talk and lp)
 */

#ifndef UTX_H
#define UTX_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

struct utx {
    int type;                   /* ut_type */
    char user[260], line[260], id[16], host[300];
    pid_t pid;
    time_t time;
    bool has_exit;              /* the next two are known */
    int term, exit;             /* of a DEAD_PROCESS */
};

/* start reading the database: file names a file of records, else the
   file in $POSIX_UTMPX (a hook for tests, not when set-user-ID or
   set-group-ID), else the system's database; false with errno set */
bool utx_open(const char *file);
/* the next entry; false at the end (errno set on a read error, else 0) */
bool utx_next(struct utx *u);
void utx_close(void);

/* the directory of the terminal lines: $POSIX_DEVDIR (as above), or /dev */
const char *utx_devdir(void);
/* the pathname of a line (malloc'd): itself when absolute */
char *utx_ttypath(const char *line);
/* a terminal's name as in the database: its pathname less the directory
   of the lines, or less /dev (pointing into path) */
const char *utx_linename(const char *path);
/* the value of an environment variable for a hook, or NULL */
const char *utx_hook(const char *name);

#endif
