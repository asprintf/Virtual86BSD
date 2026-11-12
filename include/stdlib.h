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

#ifndef _STDLIB_H
#define _STDLIB_H
#include <bits/features.h>
#include <bits/null.h>
#define __NEED_size_t
#define __NEED_wchar_t
#if __EXT_VISIBLE
#define __NEED_uint32_t
#endif
#include <bits/alltypes.h>
#if __POSIX_VISIBLE
#include <bits/waitstatus.h>
#include <bits/fcntl.h>
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define RAND_MAX 0x7fffffff
#define MB_CUR_MAX (__mb_cur_max())

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

__BEGIN_DECLS
size_t __mb_cur_max(void);

__NORETURN void _Exit(int);
__NORETURN void abort(void);
int        abs(int);
int        atexit(void (*)(void));
double     atof(const char *);
int        atoi(const char *);
long       atol(const char *);
long long  atoll(const char *);
void      *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
void      *calloc(size_t, size_t);
div_t      div(int, int);
__NORETURN void exit(int);
void       free(void *);
char      *getenv(const char *);
long       labs(long);
ldiv_t     ldiv(long, long);
long long  llabs(long long);
lldiv_t    lldiv(long long, long long);
void      *malloc(size_t);
int        mblen(const char *, size_t);
size_t     mbstowcs(wchar_t *__RESTRICT, const char *__RESTRICT, size_t);
int        mbtowc(wchar_t *__RESTRICT, const char *__RESTRICT, size_t);
void       qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int        rand(void);
void      *realloc(void *, size_t);
void       srand(unsigned);
double     strtod(const char *__RESTRICT, char **__RESTRICT);
float      strtof(const char *__RESTRICT, char **__RESTRICT);
long       strtol(const char *__RESTRICT, char **__RESTRICT, int);
long double strtold(const char *__RESTRICT, char **__RESTRICT);
long long  strtoll(const char *__RESTRICT, char **__RESTRICT, int);
unsigned long strtoul(const char *__RESTRICT, char **__RESTRICT, int);
unsigned long long strtoull(const char *__RESTRICT, char **__RESTRICT, int);
int        system(const char *);
size_t     wcstombs(char *__RESTRICT, const wchar_t *__RESTRICT, size_t);
int        wctomb(char *, wchar_t);
#if __C11_VISIBLE
void      *aligned_alloc(size_t, size_t);
int        at_quick_exit(void (*)(void));
__NORETURN void quick_exit(int);
#endif

#if __POSIX_VISIBLE
int        getsubopt(char **__RESTRICT, char *const *__RESTRICT, char **__RESTRICT);
char      *mkdtemp(char *);
int        mkostemp(char *, int);
int        mkstemp(char *);
int        posix_memalign(void **, size_t, size_t);
void       qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *);
void      *reallocarray(void *, size_t, size_t);
char      *realpath(const char *__RESTRICT, char *__RESTRICT);
char      *secure_getenv(const char *);
int        setenv(const char *, const char *, int);
int        unsetenv(const char *);
#endif
#if __XSI_VISIBLE
long       a64l(const char *);
double     drand48(void);
double     erand48(unsigned short [3]);
int        grantpt(int);
char      *initstate(unsigned, char *, size_t);
long       jrand48(unsigned short [3]);
char      *l64a(long);
void       lcong48(unsigned short [7]);
long       lrand48(void);
long       mrand48(void);
long       nrand48(unsigned short [3]);
int        posix_openpt(int);
char      *ptsname(int);
int        ptsname_r(int, char *, size_t);
int        putenv(char *);
long       random(void);
unsigned short *seed48(unsigned short [3]);
char      *setstate(char *);
void       srand48(long);
void       srandom(unsigned);
int        unlockpt(int);
#endif
#if __XSI_VISIBLE
void       setkey(const char *);
#endif
#if __EXT_VISIBLE
int        getloadavg(double [], int);
uint32_t   arc4random(void);
void       arc4random_buf(void *, size_t);
uint32_t   arc4random_uniform(uint32_t);
const char *getprogname(void);
void       setprogname(const char *);
long long  strtonum(const char *, long long, long long, const char **);
#endif
__END_DECLS
#endif
