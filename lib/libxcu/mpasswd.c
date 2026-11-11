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
 * mpasswd.c - see mpasswd.h
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "mpasswd.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* split a line into its ten fields (in place): the number of them */
static int split(char *line, char *f[10])
{
    int n = 0;
    char *p = line;
    for (;;) {
        f[n++] = p;
        p = strchr(p, ':');
        if (!p || n == 10)
            break;
        *p++ = 0;
    }
    return n;
}

static int copyfield(char *dst, size_t size, const char *s)
{
    if (strlen(s) >= size)
        return -1;
    strcpy(dst, s);
    return 0;
}

static int number(const char *s, long *v)
{
    char *end;
    if (!*s) {
        *v = 0;
        return 0;
    }
    errno = 0;
    *v = strtol(s, &end, 10);
    return (*end || errno || *v < 0) ? -1 : 0;
}

/* fill out from a line of ten fields (modified); 0, or -1 if it is malformed */
static int parse(char *line, struct spw *out)
{
    char *f[10];
    long uid, gid, change, expire;
    size_t l = strlen(line);
    if (l && line[l - 1] == '\n')
        line[l - 1] = 0;
    if (split(line, f) != 10)
        return -1;
    if (number(f[2], &uid) || number(f[3], &gid) || number(f[5], &change) || number(f[6], &expire))
        return -1;
    memset(out, 0, sizeof *out);
    if (copyfield(out->name, sizeof out->name, f[0]) || copyfield(out->hash, sizeof out->hash, f[1])
        || copyfield(out->cls, sizeof out->cls, f[4]) || copyfield(out->gecos, sizeof out->gecos, f[7])
        || copyfield(out->dir, sizeof out->dir, f[8]) || copyfield(out->shell, sizeof out->shell, f[9]))
        return -1;
    out->uid = uid;
    out->gid = gid;
    out->change = (time_t)change;
    out->expire = (time_t)expire;
    return 0;
}

int spw_lookup(const char *file, const char *name, struct spw *out)
{
    FILE *fp = fopen(file, "r");
    char *line = NULL;
    size_t cap = 0;
    int found = 0;
    if (!fp)
        return -1;
    while (getline(&line, &cap, fp) > 0) {
        struct spw e;
        char *colon;
        if (line[0] == '#' || line[0] == '\n')
            continue;
        colon = strchr(line, ':');
        if (!colon || (size_t)(colon - line) != strlen(name) || strncmp(line, name, strlen(name)))
            continue;
        if (parse(line, &e) == 0) {
            *out = e;
            found = 1;
            break;
        }
    }
    free(line);
    fclose(fp);
    return found;
}

int spw_sethash(const char *file, const char *name, const char *hash)
{
    FILE *in, *out;
    char *line = NULL, tmp[512];
    size_t cap = 0, nl = strlen(name);
    int fd, found = 0, e = 0;
    struct stat st;

    if (strchr(hash, ':') || strchr(hash, '\n') || snprintf(tmp, sizeof tmp, "%s.ptmp", file) >= (int)sizeof tmp) {
        errno = EINVAL;
        return -1;
    }
    in = fopen(file, "r");
    if (!in)
        return -1;
    if (fstat(fileno(in), &st) == -1) {
        e = errno;
        fclose(in);
        errno = e;
        return -1;
    }
    fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        e = errno;
        fclose(in);
        errno = e;
        return -1;
    }
    out = fdopen(fd, "w");
    if (!out) {
        e = errno;
        close(fd);
        unlink(tmp);
        fclose(in);
        errno = e;
        return -1;
    }
    while (getline(&line, &cap, in) > 0) {
        char *colon = strchr(line, ':');
        if (line[0] != '#' && colon && (size_t)(colon - line) == nl && !strncmp(line, name, nl) && !found) {
            char *rest = strchr(colon + 1, ':'); /* the end of the passwd field */
            if (rest) {
                fprintf(out, "%.*s:%s%s", (int)(colon - line), line, hash, rest);
                found = 1;
                continue;
            }
        }
        fputs(line, out);
    }
    free(line);
    fclose(in);
    if (!found)
        e = ESRCH;
    if (!e && (fflush(out) || fchmod(fd, st.st_mode & 07777) == -1 || (geteuid() == 0 && fchown(fd, st.st_uid, st.st_gid) == -1) || fsync(fd) == -1))
        e = errno;
    if (fclose(out) == EOF && !e)
        e = errno;
    if (!e && rename(tmp, file) == -1)
        e = errno;
    if (e) {
        unlink(tmp);
        errno = e;
        return -1;
    }
    return 0;
}
