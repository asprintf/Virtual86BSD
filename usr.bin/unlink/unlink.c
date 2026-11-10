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
 * unlink.c - unlink: call the unlink function (POSIX.1-2024, XCU unlink) [XSI]
 *
 * Synopsis: unlink file
 *
 * Calls unlink(file) and exits 0 on success, >0 on error.
 * No options are defined; -- is accepted as the end-of-options delimiter
 * per XBD 12.2 Utility Syntax Guidelines.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr, "usage: unlink file\n");
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    int opt;
    while ((opt = getopt(argc, argv, "")) != -1) {
        (void)opt;   /* getopt already wrote "illegal option" to stderr */
        usage();
        return 1;
    }
    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage();
        return 1;
    }

    if (unlink(argv[0]) != 0) {
        fprintf(stderr, "unlink: %s: %s\n", argv[0], strerror(errno));
        return 1;
    }

    return 0;
}
