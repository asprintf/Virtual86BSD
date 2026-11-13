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

#ifndef _REGEX_H
#define _REGEX_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

typedef long regoff_t;

typedef struct {
	size_t re_nsub;
	void *__re_prog;
	int __re_flags;
} regex_t;

typedef struct {
	regoff_t rm_so;
	regoff_t rm_eo;
} regmatch_t;

#define REG_EXTENDED 1
#define REG_ICASE    2
#define REG_NEWLINE  4
#define REG_NOSUB    8
#define REG_MINIMAL  16

#define REG_NOTBOL   1
#define REG_NOTEOL   2
/* Extension (BSD, glibc): match in [pmatch[0].rm_so, pmatch[0].rm_eo)
 * of string, which may contain null bytes. Offsets stay relative to
 * string, and so do the anchors: '^' matches at rm_so only if rm_so is 0. */
#define REG_STARTEND 4

#define REG_NOMATCH  1
#define REG_BADPAT   2
#define REG_ECOLLATE 3
#define REG_ECTYPE   4
#define REG_EESCAPE  5
#define REG_ESUBREG  6
#define REG_EBRACK   7
#define REG_EPAREN   8
#define REG_EBRACE   9
#define REG_BADBR    10
#define REG_ERANGE   11
#define REG_ESPACE   12
#define REG_BADRPT   13
#if __POSIX_RANGE(1L, 200809L)
/* POSIX.2 to SUSv3; never returned */
#define REG_ENOSYS   14
#endif

__BEGIN_DECLS
int    regcomp(regex_t *__RESTRICT, const char *__RESTRICT, int);
size_t regerror(int, const regex_t *__RESTRICT, char *__RESTRICT, size_t);
int    regexec(const regex_t *__RESTRICT, const char *__RESTRICT, size_t, regmatch_t *__RESTRICT, int);
void   regfree(regex_t *);
__END_DECLS
#endif
