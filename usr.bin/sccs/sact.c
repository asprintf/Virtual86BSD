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
 * sact.c - sact: print the current SCCS file-editing activity
 * (POSIX.1-2024, XCU sact)
 *
 *   sact file...
 */

#include "sccs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool multi;

static void do_file(const char *path)
{
    if (multi)
        printf("\n%s:\n", path);
    if (access(path, R_OK) != 0) {
        err("%s: %s", path, strerror(errno));
        return;
    }
    char *pname = aux_name(path, 'p');
    struct pfile pf;
    if (pfile_read(pname, &pf))
        for (int i = 0; i < pf.n; i++)
            printf("%s %s %s %s %s\n", pf.e[i].got, pf.e[i].newsid, pf.e[i].user, pf.e[i].date,
                   pf.e[i].time);
    pfile_free(&pf);
    free(pname);
}

int sact_main(int argc, char **argv)
{
    int first = 1;
    if (argc > 1 && strcmp(argv[1], "--") == 0)
        first = 2;
    if (first >= argc) {
        fprintf(stderr, "usage: sact file...\n");
        return 2;
    }
    for_each_file(argc - first, argv + first, do_file, &multi);
    if (fflush(stdout) != 0)
        return 1;
    return status;
}
