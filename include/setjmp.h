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

#ifndef _SETJMP_H
#define _SETJMP_H
#include <bits/features.h>

/* Layout as glibc's on x86-64: registers, a flag, the signal mask. */
typedef struct __jmp_buf_tag {
	unsigned long __jb[8];
	int __fl;
	unsigned long __ss[16];
} jmp_buf[1];

#if __POSIX_VISIBLE
typedef jmp_buf sigjmp_buf;
#endif

#if defined(__GNUC__) || defined(__clang__)
#define __RETURNS_TWICE __attribute__((__returns_twice__))
#else
#define __RETURNS_TWICE
#endif

__BEGIN_DECLS
int setjmp(jmp_buf) __RETURNS_TWICE;
__NORETURN void longjmp(jmp_buf, int);
#if __POSIX_VISIBLE
int sigsetjmp(sigjmp_buf, int) __RETURNS_TWICE;
__NORETURN void siglongjmp(sigjmp_buf, int);
#endif
__END_DECLS
#define setjmp setjmp
#endif
