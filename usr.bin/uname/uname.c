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
 * uname.c - uname: return system name (POSIX.1-2024, XCU uname)
 *
 * Synopsis: uname [-amnrsv]
 *
 * The fields of uname(): sysname, nodename, release, version, machine, in
 * that order whatever the order of the options, separated by single
 * spaces.  -a is all five; no options is -s.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bool want[5] = { false };   /* s n r v m */
    bool any = false;
    int c;
    while ((c = getopt(argc, argv, "amnrsv")) != -1) {
        switch (c) {
        case 'a': want[0] = want[1] = want[2] = want[3] = want[4] = true; break;
        case 's': want[0] = true; break;
        case 'n': want[1] = true; break;
        case 'r': want[2] = true; break;
        case 'v': want[3] = true; break;
        case 'm': want[4] = true; break;
        default:
            fprintf(stderr, "usage: uname [-amnrsv]\n");
            return 2;
        }
        any = true;
    }
    if (optind < argc) {
        fprintf(stderr, "usage: uname [-amnrsv]\n");
        return 2;
    }
    if (!any)
        want[0] = true;
    struct utsname u;
    if (uname(&u) < 0) {
        fprintf(stderr, "uname: %s\n", strerror(errno));
        return 1;
    }
    const char *f[5] = { u.sysname, u.nodename, u.release, u.version, u.machine };
    bool first = true;
    for (int i = 0; i < 5; i++)
        if (want[i]) {
            printf("%s%s", first ? "" : " ", f[i]);
            first = false;
        }
    putchar('\n');
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "uname: write error: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
