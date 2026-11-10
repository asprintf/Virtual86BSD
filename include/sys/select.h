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

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <bits/features.h>
#define __NEED_time_t
#define __NEED_suseconds_t
#define __NEED_sigset_t
#define __NEED_struct_timespec
#define __NEED_struct_timeval
#include <bits/alltypes.h>

#define FD_SETSIZE 1024

typedef struct {
	unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

#define __FD_WORD(d) ((d) / (8 * sizeof(long)))
#define __FD_BIT(d)  (1UL << ((d) % (8 * sizeof(long))))
#define FD_ZERO(s)  do { int __i; for (__i = 0; __i < (int)(FD_SETSIZE / (8 * sizeof(long))); __i++) \
                         (s)->fds_bits[__i] = 0; } while (0)
#define FD_SET(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] |= __FD_BIT(d)))
#define FD_CLR(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] &= ~__FD_BIT(d)))
#define FD_ISSET(d, s) (!!((s)->fds_bits[__FD_WORD(d)] & __FD_BIT(d)))

__BEGIN_DECLS
int pselect(int, fd_set *__RESTRICT, fd_set *__RESTRICT, fd_set *__RESTRICT,
            const struct timespec *__RESTRICT, const sigset_t *__RESTRICT);
int select(int, fd_set *__RESTRICT, fd_set *__RESTRICT, fd_set *__RESTRICT,
           struct timeval *__RESTRICT);
__END_DECLS
#endif
