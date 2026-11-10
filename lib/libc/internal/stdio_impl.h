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

/* Internal: the FILE object.
 *
 * Buffer pointers: reading uses rpos..rend, writing wbase..wpos..wend; at
 * most one of the two is active.  UNGET bytes before buf are reserved so
 * that ungetc() always has room.  Streams are locked with a recursive
 * lock (flockfile semantics); the *_unlocked paths skip it.
 */
#ifndef _INTERNAL_STDIO_IMPL_H
#define _INTERNAL_STDIO_IMPL_H
#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>

#define UNGET 8

#define F_PERM 1    /* stdin/stdout/stderr: never freed */
#define F_NORD 4    /* not open for reading */
#define F_NOWR 8    /* not open for writing */
#define F_EOF  16
#define F_ERR  32
#define F_SVB  64   /* setvbuf may still be called */
#define F_APP  128  /* append mode */

struct __FILE {
	unsigned flags;
	unsigned char *rpos, *rend;
	unsigned char *wend, *wpos, *wbase;
	size_t (*read)(FILE *, unsigned char *, size_t);
	size_t (*write)(FILE *, const unsigned char *, size_t);
	off_t (*seek)(FILE *, off_t, int);
	int (*close)(FILE *);
	unsigned char *buf;
	size_t buf_size;
	FILE *prev, *next;
	int fd;
	int pipe_pid;
	int lbf;                 /* line-buffering character, or -1 */
	int mode;                /* orientation: <0 byte, >0 wide, 0 none */
	volatile int lock;       /* 0 free, 1 locked, 2 contended */
	int owner;               /* tid of the lock holder */
	int lockcount;
	void *cookie;            /* memory streams */
	unsigned char *shend;    /* scanning with a limit (scanf, strtod) */
	off_t shlim, shcnt;
	char *getln_buf;
	void *mustbezero[2];
	unsigned char mbstate[8];
};

size_t __stdio_read(FILE *, unsigned char *, size_t);
size_t __stdio_write(FILE *, const unsigned char *, size_t);
size_t __stdout_write(FILE *, const unsigned char *, size_t);
off_t  __stdio_seek(FILE *, off_t, int);
int    __stdio_close(FILE *);

int  __toread(FILE *);
int  __towrite(FILE *);
int  __uflow(FILE *);
int  __overflow(FILE *, int);
size_t __fwritex(const unsigned char *, size_t, FILE *);
int  __fflush_unlocked(FILE *);
off_t __ftello_unlocked(FILE *);
int  __fseeko_unlocked(FILE *, off_t, int);
int  __fmodeflags(const char *);
FILE *__fdopen(int, const char *);
FILE *__ofl_add(FILE *);
FILE **__ofl_lock(void);
void __ofl_unlock(void);
void __stdio_exit(void);

void __lockfile(FILE *);
void __unlockfile(FILE *);
#define FLOCK(f)   __lockfile(f)
#define FUNLOCK(f) __unlockfile(f)

/* fast paths (the stream must be locked) */
#define getc_unlocked_(f) \
	(((f)->rpos != (f)->rend) ? *(f)->rpos++ : __uflow((f)))
#define putc_unlocked_(c, f) \
	((((unsigned char)(c) != (f)->lbf && (f)->wpos != (f)->wend)) \
	? *(f)->wpos++ = (unsigned char)(c) : __overflow((f), (unsigned char)(c)))

/* printf/scanf cores */
int __vfprintf(FILE *, const char *, __isoc_va_list);
int __vfscanf(FILE *, const char *, __isoc_va_list);


/* scanf input with a limit, used by the number scanners */
void __shlim(FILE *, off_t);
int  __shgetc(FILE *);
#define shcnt(f) ((f)->shcnt + ((f)->rpos - (f)->buf))
#define shlim(f, lim) __shlim((f), (lim))
#define shgetc(f) (((f)->rpos != (f)->shend) ? *(f)->rpos++ : __shgetc(f))
#define shunget(f) ((f)->shlim >= 0 ? (void)(f)->rpos-- : (void)0)

long double __floatscan(FILE *, int prec, int pok);
unsigned long long __intscan(FILE *, unsigned base, int pok, unsigned long long lim);

/* a source of wide characters for the wide scanf engine */
struct __wsrc {
	unsigned (*get)(struct __wsrc *);           /* a character, or WEOF */
	void (*unget)(struct __wsrc *, unsigned);
};
int __vfwscanf_core(struct __wsrc *, const wchar_t *, va_list);
#endif
