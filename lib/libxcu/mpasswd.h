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
 * mpasswd.h - the password database with the hashes: /etc/master.passwd,
 * mode 0600, ten fields as in the BSDs
 *     name:passwd:uid:gid:class:change:expire:gecos:dir:shell
 * (the public /etc/passwd has the same users and no hashes).  An empty
 * passwd field is an account with no password, a field that is not a
 * crypt(3) hash (`*', `!') is one nobody can log in to.  Shared by login
 * and passwd.
 */

#ifndef MPASSWD_H
#define MPASSWD_H

#include <time.h>

#define PATH_MASTER_PASSWD "/etc/master.passwd"

struct spw {
    char name[64];
    char hash[256];
    long uid, gid;
    char cls[64];
    time_t change, expire;      /* 0: never */
    char gecos[256], dir[256], shell[256];
};

/* the entry of a user: 1 found, 0 no such user, -1 error (errno) */
int spw_lookup(const char *file, const char *name, struct spw *out);

/* put hash in the passwd field of the user, the rest of the file as it
   was: written to file.ptmp (which is also the lock: created exclusively,
   mode 0600) and renamed over the file.  0, or -1 with errno: ESRCH if
   there is no such user, EEXIST if file.ptmp is there (someone else is
   at it), EINVAL for a hash with a colon or newline in it */
int spw_sethash(const char *file, const char *name, const char *hash);

#endif
