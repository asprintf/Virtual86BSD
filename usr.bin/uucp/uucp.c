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
 * uucp.c - uucp: system-to-system copy (POSIX.1-2024, XCU uucp)
 *
 * Synopsis: uucp [-cCdfjmr] [-n user] source-file... destination-file
 *
 * The UUCP system is that of uucomm.c (its configuration, spool and
 * conversation).  The copy is a job in the spool, done at once (unless
 * -r): the files are got from where they are (by a pattern, there) and
 * put where they go; a job that cannot be done because a system cannot
 * be reached stays in the spool, for a later uucp (or uux) to that
 * system or "uucico -s system" to do.
 *
 * A file name: system!path, or a path on this system ("!path" too);
 * routes (a!b!path) for the destination: each system sends the file on
 * to the next.  A path: absolute, ~user/... (the user's login directory
 * there, or the public directory if there is no such user), ~/... (in
 * the public directory), or relative: made absolute with the current
 * directory, here.  The destination is a directory when it is one, ends
 * with "/", or several files are copied: the files go in it by the last
 * part of their names.
 *
 * Another system may read and write only in its public directory (and
 * the directories of the configuration's "read" and "write").  The
 * files made: mode 0666 (0777 if the source has an execute bit), less
 * the umask.
 *
 * -c: the files are read when they are sent (the default); -C: copied
 * to the spool first.  -d: directories made as needed (the default); -f:
 * not.  -j: the job's id written.  -m: mail to the user when the job is
 * done (or has failed).  -n user: mail to the user there when a file has
 * arrived.  -r: the job is queued, not done.
 *
 * Exit status: 0 when the job was done (or queued with -r); 1 when it
 * could not be (a diagnostic: a system that cannot be reached leaves it
 * queued); 2 for usage errors.
 */

#define _POSIX_C_SOURCE 202405L

#include "uucomm.h"

#include <errno.h>
#include <glob.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(void)
{
    fputs("usage: uucp [-cCdfjmr] [-n user] source-file... destination-file\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "uucp";
    bool o_C = false, o_f = false, o_j = false, o_m = false, o_r = false;
    char *notify = NULL;
    int c;
    while ((c = getopt(argc, argv, "cCdfjmrn:")) != -1) {
        switch (c) {
        case 'c': o_C = false; break;
        case 'C': o_C = true; break;
        case 'd': o_f = false; break;
        case 'f': o_f = true; break;
        case 'j': o_j = true; break;
        case 'm': o_m = true; break;
        case 'r': o_r = true; break;
        case 'n': notify = optarg; break;
        default: usage();
        }
    }
    if (argc - optind < 2)
        usage();
    if (!uu_config())
        return 1;
    char *dsys, *dpath;
    uu_split(argv[argc - 1], &dsys, &dpath);
    if (dsys && !uu_known(dsys)) {
        fprintf(stderr, "uucp: %s: unknown system\n", dsys);
        return 1;
    }
    if (!*dpath) {
        fprintf(stderr, "uucp: %s: no file name\n", argv[argc - 1]);
        return 1;
    }
    /* a route: the path at its end is the one made absolute */
    char *dabs;
    char *last = strrchr(dpath, '!');
    if (last) {
        char *a = uu_absolute(last + 1);
        dabs = xasprintf("%.*s!%s", (int)(last - dpath), dpath, a);
        free(a);
    } else {
        dabs = uu_absolute(dpath);
    }
    struct uujob j = { 0 };
    int nsrc = argc - 1 - optind;
    /* the system of the job: the first other system */
    const char *main_sys = !uu_is_local(dsys) ? dsys : NULL;
    for (int i = optind; i < argc - 1; i++) {
        char *ssys, *spath;
        uu_split(argv[i], &ssys, &spath);
        if (ssys && !uu_known(ssys)) {
            fprintf(stderr, "uucp: %s: unknown system\n", ssys);
            return 1;
        }
        if (!*spath) {
            fprintf(stderr, "uucp: %s: no file name\n", argv[i]);
            return 1;
        }
        if (!main_sys && !uu_is_local(ssys))
            main_sys = ssys;
        j.items = xrealloc(j.items, (j.nitems + 1) * sizeof *j.items);
        j.items[j.nitems++] = (struct uuitem){ uu_is_local(ssys) ? NULL : ssys, uu_absolute(spath),
                                               uu_is_local(dsys) ? NULL : dsys, xstrdup(dabs), NULL };
        if (uu_is_local(ssys))
            free(ssys);
        free(spath);
    }
    j.id = uu_new_id(main_sys ? main_sys : conf.nodename);
    if (!j.id) {
        fprintf(stderr, "uucp: %s: cannot use the spool: %s\n", conf.spool, strerror(errno));
        return 1;
    }
    j.user = (char *)uu_user();
    j.system = (char *)(main_sys ? main_sys : conf.nodename);
    j.time = time(NULL);
    j.flags = xasprintf("%s%s%s", o_f ? "f" : "d", o_m ? "m" : "", nsrc > 1 ? "D" : "");
    j.notify = notify;
    if (o_C) {
        /* -C: the local files, copied to the spool now */
        size_t n = j.nitems;
        struct uuitem *v = j.items;
        j.items = NULL;
        j.nitems = 0;
        int k = 0;
        for (size_t i = 0; i < n; i++) {
            if (v[i].ssys) {
                j.items = xrealloc(j.items, (j.nitems + 1) * sizeof *j.items);
                j.items[j.nitems++] = v[i];
                continue;
            }
            glob_t g;
            if (glob(v[i].spath, GLOB_NOCHECK, NULL, &g) != 0) {
                fprintf(stderr, "uucp: %s: no such file\n", v[i].spath);
                return 1;
            }
            if (g.gl_pathc > 1 && !strchr(j.flags, 'D')) {
                char *f = xasprintf("%sD", j.flags);
                free(j.flags);
                j.flags = f;
            }
            for (size_t m = 0; m < g.gl_pathc; m++) {
                struct stat st;
                size_t size;
                char *d;
                if (stat(g.gl_pathv[m], &st) != 0) {
                    fprintf(stderr, "uucp: %s: %s\n", g.gl_pathv[m], strerror(errno));
                    return 1;
                }
                if (!S_ISREG(st.st_mode)) {
                    fprintf(stderr, "uucp: %s: not a regular file\n", g.gl_pathv[m]);
                    return 1;
                }
                if (!(d = uu_slurp(g.gl_pathv[m], &size))) {
                    fprintf(stderr, "uucp: %s: %s\n", g.gl_pathv[m], strerror(errno));
                    return 1;
                }
                char *data = uu_spool_data(j.id, k++, d, size);
                free(d);
                if (!data) {
                    fprintf(stderr, "uucp: %s: cannot copy to the spool: %s\n", g.gl_pathv[m], strerror(errno));
                    return 1;
                }
                chmod(data, st.st_mode & 0700);
                j.items = xrealloc(j.items, (j.nitems + 1) * sizeof *j.items);
                j.items[j.nitems++] = (struct uuitem){ NULL, xstrdup(g.gl_pathv[m]), v[i].dsys, xstrdup(v[i].dpath), data };
            }
            globfree(&g);
        }
        free(v);
    }
    if (!uu_save(&j)) {
        fprintf(stderr, "uucp: %s: cannot queue the job: %s\n", conf.spool, strerror(errno));
        return 1;
    }
    if (o_j)
        printf("%s\n", j.id);
    if (fflush(stdout) != 0) {
        fprintf(stderr, "uucp: standard output: %s\n", strerror(errno));
        return 1;
    }
    if (o_r)
        return 0;
    struct uujob *k = uu_load(j.id);
    if (!k) {
        fprintf(stderr, "uucp: %s: the job is gone\n", j.id);
        return 1;
    }
    bool temporary;
    char *err;
    if (!uu_run_job(k, &temporary, &err)) {
        fprintf(stderr, "uucp: %s%s\n", err ? err : "failed", temporary ? "; the job is queued" : "");
        return 1;
    }
    return 0;
}
