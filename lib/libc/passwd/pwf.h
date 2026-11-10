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

/* Internal: parsing of /etc/passwd and /etc/group lines. Only the files
 * are consulted (no name-service switch). */
#ifndef _INTERNAL_PWF_H
#define _INTERNAL_PWF_H
#include <pwd.h>
#include <grp.h>
#include <stdio.h>

#define PASSWD_FILE "/etc/passwd"
#define GROUP_FILE  "/etc/group"

/* Split a line in place. Return 0 on success, -1 for a malformed line.
 * __parse_gr stores up to *nmem member pointers in *mem, growing it with
 * realloc; returns -2 if that fails. */
int __parse_pw(char *line, struct passwd *);
int __parse_gr(char *line, struct group *, char ***mem, size_t *nmem);

/* Search the file: by name if name, else by id. 0 or an error number;
 * *res is null when there is no match. */
int __getpw_r(const char *name, uid_t, struct passwd *, char *, size_t, struct passwd **);
int __getgr_r(const char *name, gid_t, struct group *, char *, size_t, struct group **);

#endif
