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
 * uustat.c - uustat: uucp status inquiry and job control (POSIX.1-2024,
 * XCU uustat)
 *
 * Synopsis: uustat [-q|-k jobid|-r jobid]
 *           uustat [-s system] [-u user]
 *
 * The jobs are those of the spool of the UUCP system of uucomm.c: the
 * jobs of uucp -r and uux, and those not done because a system could not
 * be reached.
 *
 * The status of a job: "%s %s %s %s %s\n": its id, its user, its system
 * (the other system it is for, or this one), when it was made
 * (YYYY-MM-DD HH:MM) and what it does ("copy ... to ...; ...", "execute
 * ..."), and, after an attempt that could not reach a system, " (N
 * attempts: problem)".  Without options, the user's jobs; -s system:
 * those for the system; -u user: those of the user (both: both).
 *
 * -q: for each system that has jobs, "%s %d %s\n": its name, the number
 * of jobs, and the last attempt to reach it ("YYYY-MM-DD HH:MM what", or
 * "-").
 * -k jobid: the job removed (its user's, or any for a user with
 * appropriate privileges): "Job %s killed".
 * -r jobid: the job's files touched (made now): "Job %s rejuvenated".
 *
 * Exit status: 0; 1 for a job that is not there or not the user's; 2
 * for usage errors.
 */

#define _POSIX_C_SOURCE 202405L

#include "uucomm.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void usage(void)
{
    fputs("usage: uustat [-q|-k jobid|-r jobid]\n       uustat [-s system] [-u user]\n", stderr);
    exit(2);
}

static void when(time_t t, char *buf, size_t n)
{
    struct tm *tm = localtime(&t);
    if (!tm || !strftime(buf, n, "%Y-%m-%d %H:%M", tm))
        snprintf(buf, n, "%lld", (long long)t);
}

static struct uujob *own_job(const char *id)
{
    struct uujob *j = uu_load(id);
    if (!j) {
        fprintf(stderr, "uustat: %s: no such job\n", id);
        exit(1);
    }
    if (getuid() != 0 && strcmp(j->user, uu_user()) != 0) {
        fprintf(stderr, "uustat: %s: not your job\n", id);
        exit(1);
    }
    return j;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "uustat";
    bool o_q = false;
    const char *kill_id = NULL, *rej_id = NULL, *sys = NULL, *user = NULL;
    int c;
    while ((c = getopt(argc, argv, "qk:r:s:u:")) != -1) {
        switch (c) {
        case 'q': o_q = true; break;
        case 'k': kill_id = optarg; break;
        case 'r': rej_id = optarg; break;
        case 's': sys = optarg; break;
        case 'u': user = optarg; break;
        default: usage();
        }
    }
    if (optind != argc)
        usage();
    int control = o_q + (kill_id != NULL) + (rej_id != NULL);
    if (control > 1 || (control && (sys || user)))
        usage();
    if (!uu_config())
        return 1;
    if (kill_id) {
        struct uujob *j = own_job(kill_id);
        if (!uu_remove(j)) {
            fprintf(stderr, "uustat: %s: %s\n", kill_id, strerror(errno));
            return 1;
        }
        printf("Job %s killed\n", kill_id);
        uu_free_job(j);
        return fflush(stdout) == 0 ? 0 : 1;
    }
    if (rej_id) {
        struct uujob *j = own_job(rej_id);
        char *p = uu_job_path(rej_id);
        bool ok = utimensat(AT_FDCWD, p, NULL, 0) == 0;
        free(p);
        for (size_t i = 0; i < j->nitems; i++)
            if (j->items[i].data)
                utimensat(AT_FDCWD, j->items[i].data, NULL, 0);
        if (j->indata)
            utimensat(AT_FDCWD, j->indata, NULL, 0);
        if (!ok) {
            fprintf(stderr, "uustat: %s: %s\n", rej_id, strerror(errno));
            return 1;
        }
        printf("Job %s rejuvenated\n", rej_id);
        uu_free_job(j);
        return fflush(stdout) == 0 ? 0 : 1;
    }
    size_t n;
    char **ids = uu_jobs(&n);
    if (o_q) {
        /* the systems, in the order of their first jobs */
        char **names = NULL;
        int *counts = NULL;
        size_t ns = 0;
        for (size_t i = 0; i < n; i++) {
            struct uujob *j = uu_load(ids[i]);
            if (!j)
                continue;
            size_t k;
            for (k = 0; k < ns; k++)
                if (strcmp(names[k], j->system) == 0)
                    break;
            if (k == ns) {
                names = xrealloc(names, (ns + 1) * sizeof *names);
                counts = xrealloc(counts, (ns + 1) * sizeof *counts);
                names[ns] = xstrdup(j->system);
                counts[ns++] = 0;
            }
            counts[k]++;
            uu_free_job(j);
        }
        for (size_t k = 0; k < ns; k++) {
            char *st = uu_get_status(names[k]);
            if (st) {
                char *sp = strchr(st, ' ');
                char buf[64];
                when((time_t)atoll(st), buf, sizeof buf);
                printf("%s %d %s %s\n", names[k], counts[k], buf, sp ? sp + 1 : "");
                free(st);
            } else {
                printf("%s %d -\n", names[k], counts[k]);
            }
        }
    } else {
        if (!sys && !user)
            user = uu_user();
        for (size_t i = 0; i < n; i++) {
            struct uujob *j = uu_load(ids[i]);
            if (!j || (user && strcmp(j->user, user) != 0) || (sys && strcmp(j->system, sys) != 0)) {
                uu_free_job(j);
                continue;
            }
            char buf[64];
            when(j->time, buf, sizeof buf);
            char *what = uu_describe(j);
            printf("%s %s %s %s %s", j->id, j->user, j->system, buf, what);
            if (j->attempts)
                printf(" (%d attempt%s: %s)", j->attempts, j->attempts == 1 ? "" : "s", j->error ? j->error : "?");
            putchar('\n');
            free(what);
            uu_free_job(j);
        }
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "uustat: standard output: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
