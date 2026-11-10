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
 * mesg.c - mesg: permit or deny messages (POSIX.1-2024, XCU mesg)
 *
 * Synopsis: mesg [y|n]
 *
 * The terminal is the first of the standard input, output and error that
 * is one.  Messages are allowed when the group or others may write to it
 * (write and talk look at the same bits); y gives the group write
 * permission, n takes it away from the group and from others.
 *
 * Choices where the standard leaves room:
 *  - Without an operand: "is y" or "is n" on the standard output.
 *  - The exit status is that of the state after the change: 0 for y, 1
 *    for n; errors (no terminal, a bad operand, a failed change) are 2.
 *  - "--" before the operand is taken as the end of the options (there
 *    are none).
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0)
        i++;
    const char *op = NULL;
    if (argc - i > 1 || (argc - i == 1 && strcmp(argv[i], "y") != 0 && strcmp(argv[i], "n") != 0)) {
        fprintf(stderr, "usage: mesg [y|n]\n");
        return 2;
    }
    if (argc - i == 1)
        op = argv[i];

    int fd = -1;
    for (int k = 0; k < 3 && fd < 0; k++)
        if (isatty(k))
            fd = k;
    if (fd < 0) {
        fprintf(stderr, "mesg: no terminal\n");
        return 2;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        const char *n = ttyname(fd);
        fprintf(stderr, "mesg: %s: %s\n", n ? n : "terminal", strerror(errno));
        return 2;
    }
    mode_t m = st.st_mode & 07777;
    if (op) {
        mode_t nm = op[0] == 'y' ? (m | S_IWGRP) : (m & ~(mode_t)(S_IWGRP | S_IWOTH));
        if (nm != m && fchmod(fd, nm) != 0) {
            const char *n = ttyname(fd);
            fprintf(stderr, "mesg: %s: %s\n", n ? n : "terminal", strerror(errno));
            return 2;
        }
        m = nm;
    }
    int allowed = (m & (S_IWGRP | S_IWOTH)) != 0;
    if (!op) {
        printf("is %s\n", allowed ? "y" : "n");
        if (fflush(stdout) != 0 || ferror(stdout))
            return 2;
    }
    return allowed ? 0 : 1;
}
