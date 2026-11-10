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
 * rmdir.c - rmdir: remove directories (POSIX.1-2024, XCU rmdir)
 *
 * Synopsis: rmdir [-p] dir...
 *
 * -p: after dir, its parent directories as named in the operand are
 * removed, as "rmdir -p $(dirname dir)" would, until one that is a single
 * component has been removed (or a removal fails, which is reported).
 *
 * Choices where the standard leaves room:
 *  - -p stops at "." and "/", which dirname gives for the last step; they
 *    are not removed.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int status;

static bool remove_dir(const char *d)
{
    if (rmdir(d) != 0) {
        fprintf(stderr, "rmdir: %s: %s\n", d, strerror(errno));
        status = 1;
        return false;
    }
    return true;
}

/* dirname of p, in place: false if it is "." or "/" (nothing more to do) */
static bool parent(char *p)
{
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/')
        n--;
    while (n > 0 && p[n - 1] != '/')
        n--;
    if (n == 0)
        return false;           /* a single component: dirname is "." */
    while (n > 1 && p[n - 1] == '/')
        n--;
    p[n] = '\0';
    return strcmp(p, "/") != 0;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bool opt_p = false;
    int c;
    while ((c = getopt(argc, argv, "p")) != -1) {
        if (c == 'p')
            opt_p = true;
        else {
            fprintf(stderr, "usage: rmdir [-p] dir...\n");
            return 2;
        }
    }
    if (optind == argc) {
        fprintf(stderr, "usage: rmdir [-p] dir...\n");
        return 2;
    }
    for (int i = optind; i < argc; i++) {
        if (!remove_dir(argv[i]) || !opt_p)
            continue;
        char *p = strdup(argv[i]);
        if (!p) {
            fprintf(stderr, "rmdir: out of memory\n");
            return 1;
        }
        while (parent(p) && remove_dir(p))
            ;
        free(p);
    }
    return status;
}
