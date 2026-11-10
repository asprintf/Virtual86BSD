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
 * main.c - options, reading the input files, and the order of the work.
 */
#include "lex.h"

extern const char **file_names;
extern int *file_first_line;
extern int nfiles;

static void usage(void)
{
    fprintf(stderr, "usage: %s [-t] [-n|-v] [file...]\n", progname);
    exit(2);
}

static void read_all(FILE *fp, const char *name, Str *s)
{
    char buf[65536];
    size_t n;
    int line = 1;
    for (size_t i = 0; i < s->len; i++)
        if (s->s[i] == '\n')
            line++;
    file_names = xrealloc(file_names, (size_t)(nfiles + 1) * sizeof *file_names);
    file_first_line = xrealloc(file_first_line, (size_t)(nfiles + 1) * sizeof *file_first_line);
    file_names[nfiles] = name;
    file_first_line[nfiles] = line;
    nfiles++;
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0)
        str_add(s, buf, n);
    if (ferror(fp))
        fatal("cannot read %s: %s", name, strerror(errno));
    if (s->len && s->s[s->len - 1] != '\n')
        str_addc(s, '\n');
}

int main(int argc, char **argv)
{
    int opt_t = 0, opt_n = 0, opt_v = 0, i;
    Str src = { 0 };
    FILE *out;

    setlocale(LC_ALL, "");
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-' || !a[1])
            break;
        if (!strcmp(a, "--")) {
            i++;
            break;
        }
        for (const char *c = a + 1; *c; c++)
            switch (*c) {
            case 't': opt_t = 1; break;
            case 'n': opt_n = 1; opt_v = 0; break;
            case 'v': opt_v = 1; opt_n = 0; break;
            default:
                fprintf(stderr, "%s: unknown option -%c\n", progname, *c);
                usage();
            }
    }
    if (i == argc)
        read_all(stdin, "<stdin>", &src);
    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-")) {
            read_all(stdin, "<stdin>", &src);
            continue;
        }
        FILE *fp = fopen(argv[i], "r");
        if (!fp)
            fatal("cannot open %s: %s", argv[i], strerror(errno));
        read_all(fp, argv[i], &src);
        fclose(fp);
    }
    if (!src.s)
        str_adds(&src, "");

    read_source(src.s, src.len);
    if (nerrors)
        return 1;
    build_automata();

    if (opt_t) {
        out = stdout;
        out_name = "<stdout>";
    } else {
        out = fopen(out_name, "w");
        if (!out)
            fatal("cannot create %s: %s", out_name, strerror(errno));
    }
    generate(out);
    if (fflush(out) || ferror(out))
        fatal("error writing %s: %s", out_name, strerror(errno));
    if (!opt_t && fclose(out))
        fatal("error writing %s: %s", out_name, strerror(errno));

    if (opt_v && !opt_n) {
        FILE *st = opt_t ? stderr : stdout;
        int ntc = 0;
        for (int r = 0; r < nrules; r++)
            ntc += rules[r].trailing;
        fprintf(st, "%s: %d rules (%d with trailing context), %d start conditions\n", progname, nrules, ntc,
                nsconds);
        fprintf(st, "%s: %d NFA states, %d DFA states (%d before minimization), %d character classes\n", progname,
                nnfa, ndstates, ndstates_before_min, nclasses);
        fprintf(st, "%s: %ld packed transition entries (%ld unpacked)\n", progname, table_entries,
                (long)ndstates * nclasses);
        fprintf(st, "%s: no fixed table limits; %%p, %%n, %%a, %%e, %%k and %%o are accepted and ignored\n",
                progname);
    }
    return 0;
}
