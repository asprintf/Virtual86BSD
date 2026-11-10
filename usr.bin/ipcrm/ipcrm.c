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
 * ipcrm.c - ipcrm: remove an XSI message queue, semaphore set, or shared
 * memory segment identifier (POSIX.1-2024, XCU ipcrm) [XSI]
 *
 * Synopsis: ipcrm [-q msgid|-Q msgkey|-s semid|-S semkey|-m shmid|-M shmkey]...
 *
 * msgctl(), semctl() or shmctl() with IPC_RMID, for each option in the
 * order given; a key is found with msgget(), semget() or shmget() without
 * IPC_CREAT.
 *
 * Choices where the standard leaves room:
 *  - An identifier is a decimal integer; a key is an integer in C notation
 *    (decimal, 0x hexadecimal or 0 octal), as ipcs writes it in hex.  The
 *    key IPC_PRIVATE (0) names no single entry and is an error.
 *  - Each option is done even when one before it fails; the exit status is
 *    then 1.  A value that is not a number is such an error too.  Unknown
 *    options and operands are usage errors (status 2), and nothing is
 *    removed then.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

static bool number(const char *s, int base, long *v)
{
    char *end;
    if (!*s || *s == '-' || *s == '+' || *s == ' ')
        return false;
    errno = 0;
    unsigned long u = strtoul(s, &end, base);
    if (*end || errno || u > (base == 10 ? (unsigned long)INT_MAX : 0xffffffffUL))
        return false;
    *v = (long)u;
    return true;
}

static void usage(void)
{
    fprintf(stderr, "usage: ipcrm [-q msgid|-Q msgkey|-s semid|-S semkey|-m shmid|-M shmkey]...\n");
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    /* the options are checked first: a usage error removes nothing */
    int c;
    while ((c = getopt(argc, argv, "q:Q:s:S:m:M:")) != -1)
        if (c == '?' || c == ':')
            usage();
    if (optind < argc)
        usage();
    optind = 1;
    int status = 0;
    while ((c = getopt(argc, argv, "q:Q:s:S:m:M:")) != -1) {
        bool by_key = c == 'Q' || c == 'S' || c == 'M';
        long v;
        if (!number(optarg, by_key ? 0 : 10, &v)) {
            fprintf(stderr, "ipcrm: -%c %s: not %s\n", c, optarg, by_key ? "a key" : "an identifier");
            status = 1;
            continue;
        }
        int id = (int)v, r;
        if (by_key) {
            key_t key = (key_t)(unsigned)v;
            if (key == IPC_PRIVATE) {
                fprintf(stderr, "ipcrm: -%c %s: the key IPC_PRIVATE names no entry\n", c, optarg);
                status = 1;
                continue;
            }
            id = c == 'Q' ? msgget(key, 0) : c == 'S' ? semget(key, 0, 0) : shmget(key, 0, 0);
            if (id < 0) {
                fprintf(stderr, "ipcrm: -%c %s: %s\n", c, optarg,
                        errno == ENOENT ? "no entry with that key" : strerror(errno));
                status = 1;
                continue;
            }
        }
        switch (c) {
        case 'q': case 'Q': r = msgctl(id, IPC_RMID, NULL); break;
        case 's': case 'S': r = semctl(id, 0, IPC_RMID); break;
        default: r = shmctl(id, IPC_RMID, NULL); break;
        }
        if (r != 0) {
            fprintf(stderr, "ipcrm: -%c %s: %s\n", c, optarg,
                    errno == EINVAL || errno == EIDRM ? "no such identifier" : strerror(errno));
            status = 1;
        }
    }
    return status;
}
