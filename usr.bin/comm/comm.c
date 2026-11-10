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
 * comm.c - comm: select or reject lines common to two files
 * (POSIX.1-2024, XCU comm)
 *
 * Synopsis: comm [-123] file1 file2
 *
 * Lines are compared with strcoll(), and lines that collate equally but
 * are not identical by a further comparison of their bytes, as the
 * standard asks.  Lines may be of any length.
 *
 * Choices where the standard leaves room:
 *  - A last line without a newline is written with one.
 *  - A line with a NUL byte is compared up to it by strcoll() and in full
 *    by the byte comparison.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct in {
    const char *name;
    FILE *fp;
    char *line;
    size_t cap;
    ssize_t len;                /* -1 at the end */
};

static void next(struct in *f)
{
    if (f->len < 0 && f->line)
        return;
    f->len = getline(&f->line, &f->cap, f->fp);
    if (f->len < 0) {
        if (ferror(f->fp)) {
            fprintf(stderr, "comm: %s: %s\n", f->name, strerror(errno));
            exit(2);
        }
        if (!f->line)
            f->line = malloc(1);
        return;
    }
    if (f->len > 0 && f->line[f->len - 1] == '\n')
        f->line[--f->len] = '\0';
}

static int cmp(const struct in *a, const struct in *b)
{
    int c = strcoll(a->line, b->line);
    if (c)
        return c;
    size_t n = (size_t)(a->len < b->len ? a->len : b->len);
    c = memcmp(a->line, b->line, n);
    if (c)
        return c;
    return a->len < b->len ? -1 : a->len > b->len;
}

static bool show[4] = { false, true, true, true };

static void put(int col, const struct in *f)
{
    if (!show[col])
        return;
    if (col >= 2 && show[1] && putchar('\t') == EOF)
        goto bad;
    if (col == 3 && show[2] && putchar('\t') == EOF)
        goto bad;
    if (fwrite(f->line, 1, (size_t)f->len, stdout) != (size_t)f->len || putchar('\n') == EOF)
        goto bad;
    return;
bad:
    fprintf(stderr, "comm: write error: %s\n", strerror(errno));
    exit(2);
}

static FILE *open_in(const char *name)
{
    if (strcmp(name, "-") == 0)
        return stdin;
    FILE *f = fopen(name, "r");
    if (!f) {
        fprintf(stderr, "comm: %s: %s\n", name, strerror(errno));
        exit(2);
    }
    return f;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "123")) != -1) {
        if (c >= '1' && c <= '3')
            show[c - '0'] = false;
        else {
            fprintf(stderr, "usage: comm [-123] file1 file2\n");
            return 2;
        }
    }
    if (argc - optind != 2) {
        fprintf(stderr, "usage: comm [-123] file1 file2\n");
        return 2;
    }
    struct in a = { argv[optind], open_in(argv[optind]), NULL, 0, 0 };
    struct in b = { argv[optind + 1], open_in(argv[optind + 1]), NULL, 0, 0 };
    next(&a);
    next(&b);
    while (a.len >= 0 || b.len >= 0) {
        int r = a.len < 0 ? 1 : b.len < 0 ? -1 : cmp(&a, &b);
        if (r < 0) {
            put(1, &a);
            next(&a);
        } else if (r > 0) {
            put(2, &b);
            next(&b);
        } else {
            put(3, &a);
            next(&a);
            next(&b);
        }
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "comm: write error: %s\n", strerror(errno));
        return 2;
    }
    return 0;
}
