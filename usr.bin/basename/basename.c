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
 * basename.c - basename: return non-directory portion of a pathname
 *   (POSIX.1-2024, XCU basename)
 *
 * Synopsis: basename string [suffix]
 *
 * Algorithm (steps per spec):
 *  1. Empty string → '.' (implementation choice; spec allows '.' or empty)
 *  2. "//" → implementation-defined; process normally (step 3 applies)
 *  3. All slashes → output '/'
 *  4. Strip trailing slashes
 *  5. Strip directory prefix up to and including last '/'
 *  6. Strip suffix if it matches end but not the whole remaining string
 */

#define _POSIX_C_SOURCE 202405L

#include <locale.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    /* no options, but "--" as the first argument is discarded (XCU 1.4) */
    if (argc > 1 && strcmp(argv[1], "--") == 0) {
        argv++;
        argc--;
    }
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: basename string [suffix]\n");
        return 1;
    }

    /* Step 1: empty string */
    if (argv[1][0] == '\0') { puts("."); return 0; }

    char *s = argv[1];
    size_t n = strlen(s);

    /* Step 3: entirely slashes */
    size_t nslash = 0;
    while (nslash < n && s[nslash] == '/') nslash++;
    if (nslash == n) { puts("/"); return 0; }

    /* Step 4: strip trailing slashes */
    while (n > 0 && s[n - 1] == '/') n--;
    s[n] = '\0';

    /* Step 5: strip directory prefix */
    char *last = strrchr(s, '/');
    if (last) s = last + 1;

    /* Step 6: strip suffix (if given, not equal to whole string, matches end) */
    if (argc == 3) {
        const char *suffix = argv[2];
        size_t slen = strlen(s);
        size_t suflen = strlen(suffix);
        if (suflen > 0 && suflen < slen &&
            strcmp(s + slen - suflen, suffix) == 0)
            s[slen - suflen] = '\0';
    }

    if (puts(s) == EOF || fflush(stdout) == EOF)
        return 1;
    return 0;
}
