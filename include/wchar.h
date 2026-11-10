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

#ifndef _WCHAR_H
#define _WCHAR_H
#include <bits/features.h>
#include <bits/null.h>
#define __NEED_size_t
#define __NEED_wchar_t
#define __NEED_wint_t
#define __NEED_mbstate_t
#define __NEED_FILE
#define __NEED___isoc_va_list
#if __POSIX_VISIBLE
#define __NEED_va_list
#define __NEED_locale_t
#endif
#include <bits/alltypes.h>

#define WCHAR_MIN (-1 - 0x7fffffff)
#define WCHAR_MAX 0x7fffffff
#define WEOF      0xffffffffU

struct tm;

__BEGIN_DECLS
wint_t   btowc(int);
wint_t   fgetwc(FILE *);
wchar_t *fgetws(wchar_t *__RESTRICT, int, FILE *__RESTRICT);
wint_t   fputwc(wchar_t, FILE *);
int      fputws(const wchar_t *__RESTRICT, FILE *__RESTRICT);
int      fwide(FILE *, int);
int      fwprintf(FILE *__RESTRICT, const wchar_t *__RESTRICT, ...);
int      fwscanf(FILE *__RESTRICT, const wchar_t *__RESTRICT, ...);
wint_t   getwc(FILE *);
wint_t   getwchar(void);
size_t   mbrlen(const char *__RESTRICT, size_t, mbstate_t *__RESTRICT);
size_t   mbrtowc(wchar_t *__RESTRICT, const char *__RESTRICT, size_t, mbstate_t *__RESTRICT);
int      mbsinit(const mbstate_t *);
size_t   mbsrtowcs(wchar_t *__RESTRICT, const char **__RESTRICT, size_t, mbstate_t *__RESTRICT);
wint_t   putwc(wchar_t, FILE *);
wint_t   putwchar(wchar_t);
int      swprintf(wchar_t *__RESTRICT, size_t, const wchar_t *__RESTRICT, ...);
int      swscanf(const wchar_t *__RESTRICT, const wchar_t *__RESTRICT, ...);
wint_t   ungetwc(wint_t, FILE *);
int      vfwprintf(FILE *__RESTRICT, const wchar_t *__RESTRICT, __isoc_va_list);
int      vfwscanf(FILE *__RESTRICT, const wchar_t *__RESTRICT, __isoc_va_list);
int      vswprintf(wchar_t *__RESTRICT, size_t, const wchar_t *__RESTRICT, __isoc_va_list);
int      vswscanf(const wchar_t *__RESTRICT, const wchar_t *__RESTRICT, __isoc_va_list);
int      vwprintf(const wchar_t *__RESTRICT, __isoc_va_list);
int      vwscanf(const wchar_t *__RESTRICT, __isoc_va_list);
size_t   wcrtomb(char *__RESTRICT, wchar_t, mbstate_t *__RESTRICT);
wchar_t *wcscat(wchar_t *__RESTRICT, const wchar_t *__RESTRICT);
wchar_t *wcschr(const wchar_t *, wchar_t);
int      wcscmp(const wchar_t *, const wchar_t *);
int      wcscoll(const wchar_t *, const wchar_t *);
wchar_t *wcscpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT);
size_t   wcscspn(const wchar_t *, const wchar_t *);
size_t   wcsftime(wchar_t *__RESTRICT, size_t, const wchar_t *__RESTRICT, const struct tm *__RESTRICT);
size_t   wcslen(const wchar_t *);
wchar_t *wcsncat(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
int      wcsncmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *wcsncpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
wchar_t *wcsrchr(const wchar_t *, wchar_t);
size_t   wcsrtombs(char *__RESTRICT, const wchar_t **__RESTRICT, size_t, mbstate_t *__RESTRICT);
size_t   wcsspn(const wchar_t *, const wchar_t *);
wchar_t *wcsstr(const wchar_t *__RESTRICT, const wchar_t *__RESTRICT);
double   wcstod(const wchar_t *__RESTRICT, wchar_t **__RESTRICT);
float    wcstof(const wchar_t *__RESTRICT, wchar_t **__RESTRICT);
wchar_t *wcstok(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, wchar_t **__RESTRICT);
long     wcstol(const wchar_t *__RESTRICT, wchar_t **__RESTRICT, int);
long double wcstold(const wchar_t *__RESTRICT, wchar_t **__RESTRICT);
long long wcstoll(const wchar_t *__RESTRICT, wchar_t **__RESTRICT, int);
unsigned long wcstoul(const wchar_t *__RESTRICT, wchar_t **__RESTRICT, int);
unsigned long long wcstoull(const wchar_t *__RESTRICT, wchar_t **__RESTRICT, int);
size_t   wcsxfrm(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
int      wctob(wint_t);
wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
int      wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *wmemcpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t *wmemset(wchar_t *, wchar_t, size_t);
int      wprintf(const wchar_t *__RESTRICT, ...);
int      wscanf(const wchar_t *__RESTRICT, ...);

#if __POSIX_VISIBLE
size_t   mbsnrtowcs(wchar_t *__RESTRICT, const char **__RESTRICT, size_t, size_t, mbstate_t *__RESTRICT);
FILE    *open_wmemstream(wchar_t **, size_t *);
wchar_t *wcpcpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT);
wchar_t *wcpncpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
int      wcscasecmp(const wchar_t *, const wchar_t *);
int      wcscasecmp_l(const wchar_t *, const wchar_t *, locale_t);
int      wcscoll_l(const wchar_t *, const wchar_t *, locale_t);
wchar_t *wcsdup(const wchar_t *);
size_t   wcslcat(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
size_t   wcslcpy(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t);
int      wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
int      wcsncasecmp_l(const wchar_t *, const wchar_t *, size_t, locale_t);
size_t   wcsnlen(const wchar_t *, size_t);
size_t   wcsnrtombs(char *__RESTRICT, const wchar_t **__RESTRICT, size_t, size_t, mbstate_t *__RESTRICT);
size_t   wcsxfrm_l(wchar_t *__RESTRICT, const wchar_t *__RESTRICT, size_t, locale_t);
#endif
#if __XSI_VISIBLE
int      wcswidth(const wchar_t *, size_t);
int      wcwidth(wchar_t);
#endif
__END_DECLS
#endif
