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
 * sleep.c - sleep: suspend execution for an interval (POSIX.1-2024, XCU sleep)
 *
 * Synopsis: sleep time
 *
 * time is a non-negative decimal integer (seconds); any number of
 * digits is accepted, and values too large to represent sleep for the
 * longest interval that can be represented.
 * SIGALRM is effectively ignored (one of the three permitted actions);
 * other signals take their standard action.
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    /* no options, but "--" as the first argument is discarded (XCU 1.4) */
    if (argc > 1 && strcmp(argv[1], "--") == 0) {
        argv++;
        argc--;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: sleep time\n");
        return 1;
    }

    /* a decimal integer: digits only (no sign, no blanks) */
    const char *p = argv[1];
    time_t secs = 0;
    const time_t max = (time_t)((((unsigned long long)1 << (sizeof(time_t) * CHAR_BIT - 2)) - 1) * 2 + 1);
    if (!*p) {
        fprintf(stderr, "sleep: invalid time interval: '%s'\n", argv[1]);
        return 1;
    }
    for (; *p; p++) {
        if (*p < '0' || *p > '9') {
            fprintf(stderr, "sleep: invalid time interval: '%s'\n", argv[1]);
            return 1;
        }
        if (secs > (max - (*p - '0')) / 10)
            secs = max;                 /* saturate */
        else
            secs = secs * 10 + (*p - '0');
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    /* nanosleep, resumed with the time remaining if a signal interrupts it */
    struct timespec req = { secs, 0 }, rem;
    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) {
            perror("sleep");
            return 1;
        }
        req = rem;
    }
    return 0;
}
