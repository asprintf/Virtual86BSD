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
 * man.c - man: display system documentation (POSIX.1-2024, XCU man)
 *
 * Synopsis: man [-k] name...
 *
 * The pages are plain text files, one for each name, in the directories
 * of MANPATH (separated by colons; empty ones are left out), else in the
 * directory compiled in (MAN_DIR, which make sets to $(PREFIX)/share/
 * posixutilities/man).  Each directory may have a summary database,
 * whatis, of lines "name - purpose".  mkpages.tl makes both from the
 * standard's HTML pages, for every standard utility: its synopsis,
 * options, operands, environment variables and exit status, and the rest
 * of its description.
 *
 * man name writes the page of each name, from the first directory that
 * has it, the pages separated by an empty line.  man -k writes the lines
 * of the whatis files that any of the names matches, each name an
 * extended regular expression matched without regard to case (as grep
 * -Ei would).  When the standard output is a terminal, the output goes
 * through PAGER (with sh -c), or more when PAGER is unset or empty.
 *
 * Choices where the standard leaves room:
 *  - A name with no page (or with a slash, or "whatis") is reported and
 *    the status is 1; the other names are still written.
 *  - man -k with no line found writes "man: nothing appropriate" to the
 *    standard error, and the status is 1; an invalid expression is an
 *    error (status 1).
 *  - The status is also 1 when PAGER exits with a status other than 0.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef MAN_DIR
#define MAN_DIR "/usr/local/share/posixutilities/man"
#endif

static int status;
static char **dirs;
static int ndirs;

static void add_dirs(const char *list)
{
    char *copy = strdup(list);
    if (!copy) {
        fputs("man: out of memory\n", stderr);
        exit(1);
    }
    char *s = copy;
    for (;;) {
        char *c = strchr(s, ':');
        if (c)
            *c = '\0';
        if (*s) {
            dirs = realloc(dirs, (size_t)(ndirs + 1) * sizeof *dirs);
            if (!dirs) {
                fputs("man: out of memory\n", stderr);
                exit(1);
            }
            dirs[ndirs++] = s;
        }
        if (!c)
            break;
        s = c + 1;
    }
}

/* copy the file at path to out; false with a message */
static bool copy(const char *path, FILE *out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "man: %s: %s\n", path, strerror(errno));
        return false;
    }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        if (fwrite(buf, 1, n, out) != n)
            break;
    bool ok = !ferror(f);
    if (!ok)
        fprintf(stderr, "man: %s: %s\n", path, strerror(errno));
    fclose(f);
    return ok;
}

/* the page of name: its pathname in buf, or false */
static bool find_page(const char *name, char *buf, size_t size)
{
    if (!*name || strchr(name, '/') || strcmp(name, "whatis") == 0 || strcmp(name, ".") == 0
        || strcmp(name, "..") == 0)
        return false;
    for (int i = 0; i < ndirs; i++) {
        struct stat st;
        if ((size_t)snprintf(buf, size, "%s/%s", dirs[i], name) < size && stat(buf, &st) == 0
            && S_ISREG(st.st_mode))
            return true;
    }
    return false;
}

static void usage(void)
{
    fputs("usage: man [-k] name...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bool k = false;
    int c;
    while ((c = getopt(argc, argv, "k")) != -1) {
        if (c == 'k')
            k = true;
        else
            usage();
    }
    if (optind >= argc)
        usage();
    const char *mp = getenv("MANPATH");
    add_dirs(mp && *mp ? mp : MAN_DIR);

    /* the expressions of -k, checked before anything is written */
    int nre = argc - optind;
    regex_t *re = NULL;
    if (k) {
        re = calloc((size_t)nre, sizeof *re);
        if (!re) {
            fputs("man: out of memory\n", stderr);
            return 1;
        }
        for (int i = 0; i < nre; i++) {
            int e = regcomp(&re[i], argv[optind + i], REG_EXTENDED | REG_ICASE | REG_NOSUB);
            if (e != 0) {
                char msg[256];
                regerror(e, &re[i], msg, sizeof msg);
                fprintf(stderr, "man: %s: %s\n", argv[optind + i], msg);
                return 1;
            }
        }
    }

    /* the output: a pager on a terminal */
    FILE *out = stdout;
    if (isatty(1)) {
        const char *pager = getenv("PAGER");
        if (!pager || !*pager)
            pager = "more";
        fflush(stdout);
        signal(SIGPIPE, SIG_IGN);
        out = popen(pager, "w");
        if (!out) {
            fprintf(stderr, "man: %s: %s\n", pager, strerror(errno));
            return 1;
        }
    }

    char path[PATH_MAX + 1];
    if (k) {
        bool any = false;
        for (int d = 0; d < ndirs; d++) {
            if ((size_t)snprintf(path, sizeof path, "%s/whatis", dirs[d]) >= sizeof path)
                continue;
            FILE *f = fopen(path, "r");
            if (!f)
                continue;
            char *line = NULL;
            size_t cap = 0;
            ssize_t n;
            while ((n = getline(&line, &cap, f)) >= 0) {
                if (n > 0 && line[n - 1] == '\n')
                    line[n - 1] = '\0';
                for (int i = 0; i < nre; i++)
                    if (regexec(&re[i], line, 0, NULL, 0) == 0) {
                        fprintf(out, "%s\n", line);
                        any = true;
                        break;
                    }
            }
            if (ferror(f)) {
                fprintf(stderr, "man: %s: %s\n", path, strerror(errno));
                status = 1;
            }
            free(line);
            fclose(f);
        }
        if (!any) {
            fputs("man: nothing appropriate\n", stderr);
            status = 1;
        }
    } else {
        bool first = true;
        for (int i = optind; i < argc; i++) {
            if (!find_page(argv[i], path, sizeof path)) {
                fprintf(stderr, "man: no entry for %s\n", argv[i]);
                status = 1;
                continue;
            }
            if (!first)
                fputc('\n', out);
            first = false;
            if (!copy(path, out))
                status = 1;
        }
    }

    if (out != stdout) {
        int ws = pclose(out);
        if (ws == -1 || !WIFEXITED(ws) || WEXITSTATUS(ws) != 0)
            status = 1;
    } else if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "man: standard output: %s\n", strerror(errno));
        status = 1;
    }
    return status;
}
