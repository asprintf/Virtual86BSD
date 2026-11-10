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
 * what.c - what: identify SCCS files (POSIX.1-2024, XCU what)
 *
 *   what [-s] file...
 *
 * Each string after "@(#)", up to ", >, newline, \, NUL or the end.
 */

#include "sccs.h"

#include <errno.h>
#include <string.h>

int what_main(int argc, char **argv)
{
    struct opts o = { 1, 0, NULL };
    bool o_s = false, found = false;
    int c;
    while ((c = sopt(argc, argv, "s", &o)) != -1) {
        if (c != 's') {
            fprintf(stderr, "usage: what [-s] file...\n");
            return 2;
        }
        o_s = true;
    }
    if (o.ind >= argc) {
        fprintf(stderr, "usage: what [-s] file...\n");
        return 2;
    }
    int ret = 0;
    for (int i = o.ind; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            err("%s: %s", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        printf("%s:\n", argv[i]);
        static const char pat[] = "@(#)";
        int m = 0, ch;
        while ((ch = getc(f)) != EOF) {
            if (ch == pat[m]) {
                if (++m < 4)
                    continue;
                m = 0;
                putchar('\t');
                while ((ch = getc(f)) != EOF && ch != '"' && ch != '>' && ch != '\n'
                       && ch != '\\' && ch != '\0')
                    putchar(ch);
                putchar('\n');
                found = true;
                if (o_s)
                    break;
                continue;
            }
            m = ch == pat[0] ? 1 : 0;
        }
        if (ferror(f)) {
            err("%s: %s", argv[i], strerror(errno));
            ret = 1;
        }
        fclose(f);
    }
    if (fflush(stdout) != 0)
        return 1;
    return found && !ret ? 0 : 1;
}
