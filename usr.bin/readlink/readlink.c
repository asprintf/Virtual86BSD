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
 * readlink.c - readlink: display the contents of a symbolic link
 * (POSIX.1-2024, XCU readlink)
 *
 * Synopsis: readlink [-n] file
 *
 * The contents are read with readlink() into a buffer that grows until
 * they fit, so links of any length are written whole.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr, "usage: readlink [-n] file\n");
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bool opt_n = false;
    int c;
    while ((c = getopt(argc, argv, "n")) != -1) {
        if (c == 'n')
            opt_n = true;
        else
            usage();
    }
    if (argc - optind != 1)
        usage();
    const char *file = argv[optind];
    size_t cap = 256;
    char *buf = NULL;
    ssize_t n;
    for (;;) {
        if (!(buf = realloc(buf, cap))) {
            fprintf(stderr, "readlink: out of memory\n");
            return 1;
        }
        n = readlink(file, buf, cap);
        if (n < 0) {
            if (errno == EINVAL)
                fprintf(stderr, "readlink: %s: not a symbolic link\n", file);
            else
                fprintf(stderr, "readlink: %s: %s\n", file, strerror(errno));
            return 1;
        }
        if ((size_t)n < cap)
            break;
        cap *= 2;
    }
    if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n || (!opt_n && putchar('\n') == EOF) ||
        fflush(stdout) != 0) {
        fprintf(stderr, "readlink: write error: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
