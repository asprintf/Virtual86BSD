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
 * dirname.c - dirname: return the directory portion of a pathname
 *   (POSIX.1-2024, XCU dirname)
 *
 * Synopsis: dirname string
 *
 * Algorithm:
 *  1. Empty string → '.'
 *  2. Strip trailing slashes that are not also leading slashes
 *  3. No '/' left → '.'
 *  4. Strip last component (everything after the last '/')
 *  5. Strip trailing slashes from result
 *  6. Empty result → '/'
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
    if (argc != 2) {
        fprintf(stderr, "usage: dirname string\n");
        return 1;
    }

    /* Step 1: empty string */
    if (argv[1][0] == '\0') { puts("."); return 0; }

    char *s = argv[1];
    size_t n = strlen(s);

    /* Step 2: strip trailing slashes that are not leading slashes.
     * A slash at position i is a "leading slash" only if all of s[0..i] are slashes.
     * In practice: strip trailing slashes while keeping at least the leading ones. */
    /* Find first non-slash to know where leading slashes end */
    size_t lead = 0;
    while (lead < n && s[lead] == '/') lead++;

    /* Strip trailing slashes down to max(lead, 1) */
    while (n > 1 && s[n - 1] == '/' && n > lead) n--;
    s[n] = '\0';

    /* Step 3: find last slash */
    char *last = strrchr(s, '/');
    if (!last) { puts("."); return 0; }

    /* Step 4: strip last component */
    *last = '\0';
    n = (size_t)(last - s);

    /* Step 5: strip trailing slashes from result, down to 1 */
    while (n > 1 && s[n - 1] == '/') s[--n] = '\0';

    /* Step 6: empty result means root */
    if (puts(n == 0 ? "/" : s) == EOF || fflush(stdout) == EOF)
        return 1;
    return 0;
}
