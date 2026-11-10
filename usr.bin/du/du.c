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
 * du.c - du: estimate file space usage (POSIX.1-2024, XCU du)
 *
 * Synopsis: du [-a|-s] [-kx] [-H|-L] [file...]
 *
 * The hierarchy is walked by pathname, depth first; a directory's entries
 * are read and the directory closed before its subdirectories are
 * entered, so the depth is limited by PATH_MAX, not by descriptors.  A
 * directory is written after its contents.  Sizes are the st_blocks of
 * stat() (in units of 512 bytes, as on every system with it), summed and
 * written in 512-byte units, or with -k in 1024-byte units rounded up.
 *
 * Choices where the standard leaves room:
 *  - Every file is remembered by device and inode, so a file met a second
 *    time (a hard link, a directory named twice, or a loop through -L) is
 *    neither counted nor written again: the first entry met is the one
 *    written.  An operand already met writes nothing.
 *  - The entries of a directory are taken in the order of strcmp().
 *  - -a with -s: -s wins (only the totals); -H with -L: the last given.
 *  - -x leaves out the files on other devices altogether, mount points
 *    and all.
 *  - A directory that cannot be read is reported, and written with the
 *    size of what could be counted; the exit status is then 1.
 *  - A pathname with a newline in it is written as it is.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool o_a, o_s, o_k, o_x;
static int follow;              /* 0: none, 'H', 'L' */
static int status;
static dev_t opdev;

/* ---- the files met: a hash set of (dev, ino) ---- */

struct key {
    dev_t dev;
    ino_t ino;
};
static struct key *set;
static size_t setcap, setn;

static size_t hash(dev_t d, ino_t i)
{
    uint64_t h = (uint64_t)i * 0x9E3779B97F4A7C15u ^ (uint64_t)d * 0xC2B2AE3D27D4EB4Fu;
    return (size_t)(h ^ (h >> 29));
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "du: out of memory\n");
        exit(1);
    }
    return p;
}

/* true if (dev, ino) was not there (and is now) */
static bool remember(dev_t d, ino_t i)
{
    if (2 * (setn + 1) > setcap) {
        size_t nc = setcap ? 2 * setcap : 1024;
        struct key *ns = xmalloc(nc * sizeof *ns);
        bool *used = xmalloc(nc);
        memset(used, 0, nc);
        for (size_t k = 0; k < setcap; k++) {
            if (set[k].ino == 0 && set[k].dev == 0)
                continue;
            size_t h = hash(set[k].dev, set[k].ino) & (nc - 1);
            while (used[h])
                h = (h + 1) & (nc - 1);
            used[h] = true;
            ns[h] = set[k];
        }
        for (size_t k = 0; k < nc; k++)
            if (!used[k])
                ns[k].dev = 0, ns[k].ino = 0;
        free(used);
        free(set);
        set = ns;
        setcap = nc;
    }
    /* (0, 0) marks an empty slot; such a file is stored as (0, 0) too and
       so is never taken as met: counted again, which is harmless */
    if (d == 0 && i == 0)
        return true;
    size_t h = hash(d, i) & (setcap - 1);
    while (set[h].dev != 0 || set[h].ino != 0) {
        if (set[h].dev == d && set[h].ino == i)
            return false;
        h = (h + 1) & (setcap - 1);
    }
    set[h].dev = d;
    set[h].ino = i;
    setn++;
    return true;
}

/* ---- output ---- */

static uintmax_t blocks(const struct stat *st)
{
    return (uintmax_t)st->st_blocks;
}

static void report(uintmax_t b, const char *path)
{
    uintmax_t n = o_k ? b / 2 + b % 2 : b;
    printf("%ju %s\n", n, path);
}

/* ---- the walk ---- */

static char *join(const char *dir, const char *name)
{
    size_t a = strlen(dir), b = strlen(name);
    char *p = xmalloc(a + b + 2);
    memcpy(p, dir, a);
    size_t k = a;
    if (a == 0 || dir[a - 1] != '/')
        p[k++] = '/';
    memcpy(p + k, name, b + 1);
    return p;
}

static int cmpstr(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* the size of the hierarchy at path (st its status); top: an operand */
static uintmax_t walk(const char *path, const struct stat *st, bool top)
{
    if (o_x && st->st_dev != opdev)
        return 0;
    if (!remember(st->st_dev, st->st_ino))
        return 0;
    uintmax_t total = blocks(st);
    if (!S_ISDIR(st->st_mode)) {
        if (top || (o_a && !o_s))
            report(total, path);
        return total;
    }

    /* the names, read before descending */
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "du: %s: %s\n", path, strerror(errno));
        status = 1;
    } else {
        char **names = NULL;
        size_t n = 0, cap = 0;
        struct dirent *e;
        errno = 0;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            if (n == cap) {
                cap = cap ? 2 * cap : 32;
                char **nn = realloc(names, cap * sizeof *names);
                if (!nn) {
                    fprintf(stderr, "du: out of memory\n");
                    exit(1);
                }
                names = nn;
            }
            size_t l = strlen(e->d_name);
            names[n] = xmalloc(l + 1);
            memcpy(names[n++], e->d_name, l + 1);
            errno = 0;
        }
        if (errno != 0) {
            fprintf(stderr, "du: %s: %s\n", path, strerror(errno));
            status = 1;
        }
        closedir(d);
        if (n > 1)
            qsort(names, n, sizeof *names, cmpstr);
        for (size_t i = 0; i < n; i++) {
            char *sub = join(path, names[i]);
            struct stat s;
            int r = follow == 'L' ? stat(sub, &s) : lstat(sub, &s);
            if (r != 0) {
                fprintf(stderr, "du: %s: %s\n", sub, strerror(errno));
                status = 1;
            } else {
                total += walk(sub, &s, false);
            }
            free(sub);
            free(names[i]);
        }
        free(names);
    }
    if (!o_s || top)
        report(total, path);
    return total;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "askxHL")) != -1) {
        switch (c) {
        case 'a': o_a = true; break;
        case 's': o_s = true; break;
        case 'k': o_k = true; break;
        case 'x': o_x = true; break;
        case 'H': follow = 'H'; break;
        case 'L': follow = 'L'; break;
        default:
            fprintf(stderr, "usage: du [-a|-s] [-kx] [-H|-L] [file...]\n");
            return 2;
        }
    }
    char *dot[] = { ".", NULL };
    char **ops = optind < argc ? argv + optind : dot;
    for (; *ops; ops++) {
        struct stat st;
        int r = follow ? stat(*ops, &st) : lstat(*ops, &st);
        if (r != 0) {
            fprintf(stderr, "du: %s: %s\n", *ops, strerror(errno));
            status = 1;
            continue;
        }
        opdev = st.st_dev;
        walk(*ops, &st, true);
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "du: standard output: %s\n", strerror(errno));
        return 1;
    }
    return status;
}
