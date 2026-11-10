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
 * uucico.c - the UUCP system's other side and queue runner (not a
 * utility of POSIX.1-2024; for uucp, uux and uustat)
 *
 * Synopsis: uucico -S
 *           uucico [-s system]
 *
 * -S: answer a uucp or uux of another system on the standard input and
 * output (see uucomm.c): what a system's "system" line in the
 * configuration runs there.
 *
 * Without -S: do the queued jobs of the user (all of them, or those for
 * a system), as uucp and uux do theirs; a line on the standard error for
 * each job not done.  Exit status: 0 if all were done, 1 if not.
 */

#define _POSIX_C_SOURCE 202405L

#include "uucomm.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    fputs("usage: uucico -S\n       uucico [-s system]\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "uucico";
    bool slave = false;
    const char *sys = NULL;
    int c;
    while ((c = getopt(argc, argv, "Ss:")) != -1) {
        switch (c) {
        case 'S': slave = true; break;
        case 's': sys = optarg; break;
        default: usage();
        }
    }
    if (optind != argc || (slave && sys))
        usage();
    if (!uu_config())
        return 1;
    if (slave)
        return uu_slave();
    size_t n;
    char **ids = uu_jobs(&n);
    int status = 0;
    for (size_t i = 0; i < n; i++) {
        struct uujob *j = uu_load(ids[i]);
        if (!j || strcmp(j->user, uu_user()) != 0 || (sys && strcmp(j->system, sys) != 0)) {
            uu_free_job(j);
            continue;
        }
        bool temporary;
        char *err;
        if (!uu_run_job(j, &temporary, &err)) {
            fprintf(stderr, "uucico: %s: %s%s\n", ids[i], err ? err : "failed", temporary ? "; queued" : "");
            status = 1;
        }
        free(err);
        uu_free_job(j);
    }
    return status;
}
