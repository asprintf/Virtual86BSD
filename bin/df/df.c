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
 * df.c - df: report free disk space (POSIX.1-2024, XCU df)
 *
 * Synopsis: df [-k] [-P|-t] [file...]
 *
 * The figures come from statvfs(): total f_blocks, used f_blocks - f_bfree,
 * available f_bavail, each times f_frsize and rounded up to the unit (512
 * bytes, or 1024 with -k); the capacity is used / (used + available) as a
 * percentage rounded up.  The file systems are those of the mount table
 * (/proc/self/mounts, else /etc/mtab: there is no interface in POSIX for
 * it), their names its first field, with the escapes \040, \011, \012 and
 * \134 undone.
 *
 * Choices where the standard leaves room:
 *  - The default output (also with -t) has the columns of -P, the total
 *    among them, and then the free and the total file slots (inodes):
 *        Filesystem 512-blocks Used Available Capacity Ifree Inodes Mounted on
 *    The columns of both formats are aligned: the numbers to the right.
 *  - Without operands, the file systems with no blocks (proc, sysfs and
 *    their like) are left out, and so is a file system mounted again on a
 *    mount point already listed (only the last mount there is visible).
 *  - A file operand is in the file system whose mount point has the
 *    file's device and is the longest leading part of the file's real
 *    path.  A block special file that is the name of a mounted file
 *    system means that file system; one not mounted is reported as such
 *    (the free space of an unmounted file system cannot be had portably).
 *  - With operands but none of them found, not even the header is written.
 *  - -P and -t together: the -P format.  When used + available is 0 the
 *    capacity is 0%.
 *  - Usage errors exit with status 2, other errors 1.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "mnttab.h"

static bool o_k, o_P, o_t;
static int status;

static struct mnt *mnts;
static size_t nmnt;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "df: out of memory\n");
        exit(1);
    }
    return p;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    return memcpy(xmalloc(n), s, n);
}

static bool read_mounts(void)
{
    const char *const names[] = { "/proc/self/mounts", "/etc/mtab", "/etc/mnttab", NULL };
    if (!mnt_read(names, &mnts, &nmnt)) {
        fprintf(stderr, errno == ENOMEM ? "df: out of memory\n" : "df: cannot read the mount table\n");
        return false;
    }
    return true;
}

/* ---- rows, kept until all are known so the columns line up ---- */

struct row {
    char *c[8];
};
static struct row *rows;
static size_t nrows, rcap;

static char *num(uintmax_t v)
{
    char b[32];
    snprintf(b, sizeof b, "%ju", v);
    return xstrdup(b);
}

static uintmax_t units(uintmax_t count, uintmax_t size)
{
    uintmax_t u = o_k ? 1024 : 512;
    /* count * size / u rounded up, without overflow where possible */
    if (size % u == 0)
        return count * (size / u);
    if (u % size == 0) {
        uintmax_t k = u / size;
        return count / k + (count % k != 0);
    }
    long double x = (long double)count * size / u;
    uintmax_t r = (uintmax_t)x;
    return r + ((long double)r < x);
}

static void add_row(const struct mnt *m)
{
    struct statvfs v;
    if (statvfs(m->dir, &v) != 0) {
        fprintf(stderr, "df: %s: %s\n", m->dir, strerror(errno));
        status = 1;
        return;
    }
    uintmax_t fr = v.f_frsize ? v.f_frsize : v.f_bsize;
    uintmax_t total = units(v.f_blocks, fr);
    uintmax_t used = units(v.f_blocks >= v.f_bfree ? v.f_blocks - v.f_bfree : 0, fr);
    uintmax_t avail = units(v.f_bavail, fr);
    uintmax_t pct = 0;
    if (used + avail > 0)
        pct = (used * 100 + used + avail - 1) / (used + avail);
    if (nrows == rcap) {
        rcap = rcap ? 2 * rcap : 16;
        struct row *n = realloc(rows, rcap * sizeof *rows);
        if (!n) {
            fprintf(stderr, "df: out of memory\n");
            exit(1);
        }
        rows = n;
    }
    struct row *r = &rows[nrows++];
    char pb[32];
    snprintf(pb, sizeof pb, "%ju%%", pct);
    r->c[0] = xstrdup(m->fsname);
    r->c[1] = num(total);
    r->c[2] = num(used);
    r->c[3] = num(avail);
    r->c[4] = xstrdup(pb);
    if (o_P) {
        r->c[5] = xstrdup(m->dir);
        r->c[6] = r->c[7] = NULL;
    } else {
        r->c[5] = num(v.f_favail);
        r->c[6] = num(v.f_files);
        r->c[7] = xstrdup(m->dir);
    }
}

static void print_rows(void)
{
    const char *hp[] = { "Filesystem", o_k ? "1024-blocks" : "512-blocks", "Used", "Available",
                         "Capacity", "Mounted on" };
    const char *hd[] = { "Filesystem", o_k ? "1024-blocks" : "512-blocks", "Used", "Available",
                         "Capacity", "Ifree", "Inodes", "Mounted on" };
    const char **h = o_P ? hp : hd;
    int nc = o_P ? 6 : 8;
    size_t w[8];
    for (int i = 0; i < nc; i++) {
        w[i] = strlen(h[i]);
        for (size_t k = 0; k < nrows; k++) {
            size_t l = strlen(rows[k].c[i]);
            if (l > w[i])
                w[i] = l;
        }
    }
    /* the header: the first column to the left, the numbers to the right */
    printf("%-*s", (int)w[0], h[0]);
    for (int i = 1; i < nc - 1; i++)
        printf(" %*s", (int)w[i], h[i]);
    printf(" %s\n", h[nc - 1]);
    for (size_t k = 0; k < nrows; k++) {
        printf("%-*s", (int)w[0], rows[k].c[0]);
        for (int i = 1; i < nc - 1; i++)
            printf(" %*s", (int)w[i], rows[k].c[i]);
        printf(" %s\n", rows[k].c[nc - 1]);
        for (int i = 0; i < nc; i++)
            free(rows[k].c[i]);
    }
}

/* the mount for a file operand, or NULL with a message */
static const struct mnt *mount_of(const char *file)
{
    struct stat st;
    if (stat(file, &st) != 0) {
        fprintf(stderr, "df: %s: %s\n", file, strerror(errno));
        return NULL;
    }
    if (S_ISBLK(st.st_mode)) {
        for (size_t i = nmnt; i-- > 0; ) {
            struct stat ms;
            if (stat(mnts[i].fsname, &ms) == 0 && S_ISBLK(ms.st_mode) && ms.st_rdev == st.st_rdev)
                return &mnts[i];
        }
        fprintf(stderr, "df: %s: not a mounted file system\n", file);
        return NULL;
    }
    char *real = realpath(file, NULL);
    const struct mnt *best = NULL;
    bool bestprefix = false;
    size_t bestlen = 0;
    for (size_t i = 0; i < nmnt; i++) {
        struct stat ms;
        if (stat(mnts[i].dir, &ms) != 0 || ms.st_dev != st.st_dev)
            continue;
        size_t l = strlen(mnts[i].dir);
        bool prefix = real && strncmp(real, mnts[i].dir, l) == 0
                      && (real[l] == '/' || real[l] == '\0' || strcmp(mnts[i].dir, "/") == 0);
        if (prefix && (!bestprefix || l >= bestlen)) {
            best = &mnts[i];
            bestprefix = true;
            bestlen = l;
        } else if (!best) {
            best = &mnts[i];
        }
    }
    free(real);
    if (!best) {
        fprintf(stderr, "df: %s: no file system found in the mount table\n", file);
        return NULL;
    }
    return best;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "kPt")) != -1) {
        switch (c) {
        case 'k': o_k = true; break;
        case 'P': o_P = true; break;
        case 't': o_t = true; break;
        default:
            fprintf(stderr, "usage: df [-k] [-P|-t] [file...]\n");
            return 2;
        }
    }
    (void)o_t;          /* the default output has the totals already */
    if (!read_mounts())
        return 1;
    if (optind == argc) {
        for (size_t i = 0; i < nmnt; i++) {
            bool later = false;     /* mounted over by a later entry */
            for (size_t k = i + 1; k < nmnt && !later; k++)
                later = strcmp(mnts[k].dir, mnts[i].dir) == 0;
            if (later)
                continue;
            struct statvfs v;
            if (statvfs(mnts[i].dir, &v) != 0 || v.f_blocks == 0)
                continue;
            add_row(&mnts[i]);
        }
    } else {
        for (int i = optind; i < argc; i++) {
            const struct mnt *m = mount_of(argv[i]);
            if (m)
                add_row(m);
            else
                status = 1;
        }
    }
    if (nrows > 0 || optind == argc)
        print_rows();
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "df: standard output: %s\n", strerror(errno));
        return 1;
    }
    return status;
}
