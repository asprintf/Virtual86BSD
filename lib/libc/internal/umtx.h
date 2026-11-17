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

/* Internal: the kernel's userland mutex interface (_umtx_op), what the
 * library's futex waits and wakes are made of. */
#ifndef _INTERNAL_UMTX_H
#define _INTERNAL_UMTX_H
#include <time.h>
#include "syscall.h"

#define UMTX_OP_WAIT              2    /* *(long *)obj == val */
#define UMTX_OP_WAKE              3
#define UMTX_OP_MUTEX_TRYLOCK     4
#define UMTX_OP_MUTEX_LOCK        5
#define UMTX_OP_MUTEX_UNLOCK      6
#define UMTX_OP_SET_CEILING       7
#define UMTX_OP_WAIT_UINT         11   /* *(unsigned *)obj == val */
#define UMTX_OP_WAIT_UINT_PRIVATE 15
#define UMTX_OP_WAKE_PRIVATE      16
#define UMTX_OP_MUTEX_WAIT        17
#define UMTX_OP_MUTEX_WAKE2       22
#define UMTX_OP_ROBUST_LISTS      26

#define UMTX_ABSTIME 1

/* the timeout of a wait: an absolute or relative time on a clock; its size
 * goes in uaddr1, else uaddr2 is a plain struct timespec (relative) */
struct _umtx_time {
	struct timespec _timeout;
	unsigned int _flags;
	unsigned int _clockid;
};

static inline long __umtx_op(void *obj, int op, unsigned long val, void *a1, void *a2)
{
	return __syscall5(SYS__umtx_op, obj, op, val, a1, a2);
}

#endif
