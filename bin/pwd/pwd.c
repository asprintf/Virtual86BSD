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
 * pwd.c - pwd: return working directory name (POSIX.1-2024, XCU pwd)
 *
 * Synopsis: pwd [-L|-P]
 *
 * -L (default): use $PWD if it is a valid absolute path naming the cwd
 * -P:           resolve symlinks via getcwd()
 */

#define _POSIX_C_SOURCE 202405L

#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Return true if path contains no '.' or '..' components. */
static bool no_dot(const char *p)
{
    while (*p) {
        while (*p == '/') p++;
        if (p[0] == '.' && (p[1] == '/' || p[1] == '\0')) return false;
        if (p[0] == '.' && p[1] == '.' &&
            (p[2] == '/' || p[2] == '\0')) return false;
        while (*p && *p != '/') p++;
    }
    return true;
}

#include <errno.h>

static void out(const char *s)
{
    if (puts(s) == EOF || fflush(stdout) == EOF) {
        perror("pwd: write error");
        exit(1);
    }
}

/* the physical pathname: getcwd() with a buffer that grows, so that
   directories deeper than PATH_MAX bytes work where the system allows */
static void pwd_physical(void)
{
    size_t cap = PATH_MAX;
    char *buf = NULL;
    for (;;) {
        char *nb = realloc(buf, cap);
        if (!nb) {
            perror("pwd");
            exit(1);
        }
        buf = nb;
        if (getcwd(buf, cap) != NULL)
            break;
        if (errno != ERANGE) {
            perror("pwd");
            exit(1);
        }
        cap *= 2;
    }
    out(buf);
    free(buf);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    bool logical = true; /* default: -L */

    int opt;
    while ((opt = getopt(argc, argv, "LP")) != -1) {
        switch (opt) {
        case 'L': logical = true;  break;
        case 'P': logical = false; break;
        default:
            fprintf(stderr, "usage: pwd [-L|-P]\n");
            return 1;
        }
    }

    if (!logical) { pwd_physical(); return 0; }

    /* -L: try $PWD first */
    const char *env = getenv("PWD");
    if (env && env[0] == '/' && no_dot(env) &&
        strlen(env) < PATH_MAX) {
        struct stat es, cs;
        if (stat(env, &es) == 0 && stat(".", &cs) == 0 &&
            es.st_dev == cs.st_dev && es.st_ino == cs.st_ino) {
            out(env);
            return 0;
        }
    }

    /* Fall back to physical */
    pwd_physical();
    return 0;
}
