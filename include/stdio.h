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

#ifndef _STDIO_H
#define _STDIO_H
#include <bits/features.h>
#include <bits/null.h>
#define __NEED_size_t
#define __NEED___isoc_va_list
#define __NEED_FILE
#if __POSIX_VISIBLE
#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_va_list
#endif
#include <bits/alltypes.h>

typedef union { char __opaque[16]; long long __align; } fpos_t;

#define BUFSIZ       8192
#define EOF          (-1)
#define FILENAME_MAX 4096
#define FOPEN_MAX    1000
#define L_tmpnam     20
#define TMP_MAX      308915776

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#if __POSIX_VISIBLE
#define L_ctermid 20
#endif
#if __XSI_OB5
#define L_cuserid 256           /* LOGIN_NAME_MAX: SUSv2 (LEGACY) */
#endif
#if __XSI_OB
#define P_tmpdir "/tmp"         /* SUSv2 to SUSv4 */
#endif

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;
#define stdin  (stdin)
#define stdout (stdout)
#define stderr (stderr)

__BEGIN_DECLS
void   clearerr(FILE *);
int    fclose(FILE *);
int    feof(FILE *);
int    ferror(FILE *);
int    fflush(FILE *);
int    fgetc(FILE *);
int    fgetpos(FILE *__RESTRICT, fpos_t *__RESTRICT);
char  *fgets(char *__RESTRICT, int, FILE *__RESTRICT);
FILE  *fopen(const char *__RESTRICT, const char *__RESTRICT);
int    fprintf(FILE *__RESTRICT, const char *__RESTRICT, ...);
int    fputc(int, FILE *);
int    fputs(const char *__RESTRICT, FILE *__RESTRICT);
size_t fread(void *__RESTRICT, size_t, size_t, FILE *__RESTRICT);
FILE  *freopen(const char *__RESTRICT, const char *__RESTRICT, FILE *__RESTRICT);
int    fscanf(FILE *__RESTRICT, const char *__RESTRICT, ...);
int    fseek(FILE *, long, int);
int    fsetpos(FILE *, const fpos_t *);
long   ftell(FILE *);
size_t fwrite(const void *__RESTRICT, size_t, size_t, FILE *__RESTRICT);
int    getc(FILE *);
int    getchar(void);
void   perror(const char *);
int    printf(const char *__RESTRICT, ...);
int    putc(int, FILE *);
int    putchar(int);
int    puts(const char *);
int    remove(const char *);
int    rename(const char *, const char *);
void   rewind(FILE *);
int    scanf(const char *__RESTRICT, ...);
void   setbuf(FILE *__RESTRICT, char *__RESTRICT);
int    setvbuf(FILE *__RESTRICT, char *__RESTRICT, int, size_t);
int    snprintf(char *__RESTRICT, size_t, const char *__RESTRICT, ...);
int    sprintf(char *__RESTRICT, const char *__RESTRICT, ...);
int    sscanf(const char *__RESTRICT, const char *__RESTRICT, ...);
FILE  *tmpfile(void);
char  *tmpnam(char *);
int    ungetc(int, FILE *);
int    vfprintf(FILE *__RESTRICT, const char *__RESTRICT, __isoc_va_list);
int    vfscanf(FILE *__RESTRICT, const char *__RESTRICT, __isoc_va_list);
int    vprintf(const char *__RESTRICT, __isoc_va_list);
int    vscanf(const char *__RESTRICT, __isoc_va_list);
int    vsnprintf(char *__RESTRICT, size_t, const char *__RESTRICT, __isoc_va_list);
int    vsprintf(char *__RESTRICT, const char *__RESTRICT, __isoc_va_list);
int    vsscanf(const char *__RESTRICT, const char *__RESTRICT, __isoc_va_list);

#if __POSIX_VISIBLE
int    asprintf(char **__RESTRICT, const char *__RESTRICT, ...);
char  *ctermid(char *);
int    dprintf(int, const char *__RESTRICT, ...);
FILE  *fdopen(int, const char *);
int    fileno(FILE *);
void   flockfile(FILE *);
FILE  *fmemopen(void *__RESTRICT, size_t, const char *__RESTRICT);
int    fseeko(FILE *, off_t, int);
off_t  ftello(FILE *);
int    ftrylockfile(FILE *);
void   funlockfile(FILE *);
int    getc_unlocked(FILE *);
int    getchar_unlocked(void);
ssize_t getdelim(char **__RESTRICT, size_t *__RESTRICT, int, FILE *__RESTRICT);
ssize_t getline(char **__RESTRICT, size_t *__RESTRICT, FILE *__RESTRICT);
FILE  *open_memstream(char **, size_t *);
int    pclose(FILE *);
FILE  *popen(const char *, const char *);
int    putc_unlocked(int, FILE *);
int    putchar_unlocked(int);
int    renameat(int, const char *, int, const char *);
int    vasprintf(char **__RESTRICT, const char *__RESTRICT, va_list);
int    vdprintf(int, const char *__RESTRICT, va_list);
#endif
#if __EXT_VISIBLE
int    fpurge(FILE *);
#endif
#if __XSI_OB
/* SUSv2 to SUSv4 (obsolescent there); removed in POSIX.1-2024 */
char  *tempnam(const char *, const char *);
#endif
#if __XSI_OB5
/* SUSv2 (LEGACY) */
char  *cuserid(char *);
int    getopt(int, char *const [], const char *);
extern char *optarg;
extern int opterr, optind, optopt;
int    getw(FILE *);
int    putw(int, FILE *);
#endif
__END_DECLS
#endif
