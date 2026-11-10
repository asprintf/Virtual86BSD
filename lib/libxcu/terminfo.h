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
 * terminfo.h - reading compiled terminfo entries, and their parameter
 * strings (shared by tput, tabs and talk)
 */

#ifndef TERMINFO_H
#define TERMINFO_H

#include <stdbool.h>
#include <stddef.h>

struct terminfo {
    char *names;                /* "name|alias|...|long name" */
    signed char *bools;
    int nbools;
    long *nums;
    int nnums;
    char **strs;                /* NULL: absent */
    int nstrs;
    /* the extended capabilities: names, then values as above */
    char **xnames;
    signed char *xbools;
    int nxbools;
    long *xnums;
    int nxnums;
    char **xstrs;
    int nxstrs;
};

/* load the entry of terminal type name: 0, or -1 when there is none, or
   -2 when the file found is not a terminfo entry */
int ti_load(const char *name, struct terminfo *t);

/* a capability by its name: TI_ABSENT, or its kind and value */
enum ti_kind { TI_ABSENT, TI_BOOL, TI_NUM, TI_STR };
enum ti_kind ti_find(const struct terminfo *t, const char *cap, long *num, const char **str);

/* the string s with the parameters p[0..8] (strings where isstr[i]);
   malloc()ed, NULL when out of memory */
char *ti_tparm(const char *s, const long p[9], char *const sp[9]);

/* the parameters a parameterized string uses: the highest %pN, and for
   each one whether it is used with %s or %l (a string) */
int ti_nparams(const char *s, bool isstr[9]);

/* write s to the standard output without its padding ($<...>); false on
   a write error */
bool ti_put(const char *s);

/* the long name: the last of the names */
const char *ti_longname(const struct terminfo *t);

#endif
