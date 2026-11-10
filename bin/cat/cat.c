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
 * cat.c - cat: concatenate and print files (POSIX.1-2024, XCU cat)
 *
 * Synopsis: cat [-u] [file...]
 *
 * Input is copied with read() and write(), so whatever one read() returns
 * is written at once: output is never delayed, as -u requires (and
 * without -u it is no different).  "-" is the standard input each time
 * it appears; it is not closed and reopened.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int had_error;

static int write_all(const char *buf, size_t n)
{
    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, buf, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += w;
        n -= (size_t)w;
    }
    return 0;
}

static void cat_fd(int fd, const char *name)
{
    static char buf[65536];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n == 0)
            return;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
            had_error = 1;
            return;
        }
        if (write_all(buf, (size_t)n) != 0) {
            fprintf(stderr, "cat: write error: %s\n", strerror(errno));
            exit(1);
        }
    }
}

static void cat_file(const char *path)
{
    if (strcmp(path, "-") == 0) {
        cat_fd(STDIN_FILENO, "standard input");
        return;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        had_error = 1;
        return;
    }
    cat_fd(fd, path);
    close(fd);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    int opt;
    while ((opt = getopt(argc, argv, "u")) != -1) {
        switch (opt) {
        case 'u':
            break;      /* output is never delayed */
        default:
            fprintf(stderr, "usage: cat [-u] [file...]\n");
            return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
        cat_fd(STDIN_FILENO, "standard input");
    else
        for (int i = 0; i < argc; i++)
            cat_file(argv[i]);

    return had_error ? 1 : 0;
}
