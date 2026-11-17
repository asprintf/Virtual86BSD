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

/* Futex waits and wakes, on _umtx_op.  An absolute timeout is measured
 * against CLOCK_REALTIME or CLOCK_MONOTONIC by the kernel (_umtx_time). */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "umtx.h"

static int fwait(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv, int cp)
{
	long r;
	int op = priv ? UMTX_OP_WAIT_UINT_PRIVATE : UMTX_OP_WAIT_UINT;
	struct _umtx_time t;
	void *a1 = 0, *a2 = 0;
	if (at) {
		if ((unsigned long)at->tv_nsec >= 1000000000UL)
			return EINVAL;
		if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
			return EINVAL;
		if (at->tv_sec < 0)
			return ETIMEDOUT;
		t._timeout = *at;
		t._flags = UMTX_ABSTIME;
		t._clockid = (unsigned)clk;
		a1 = (void *)sizeof t;
		a2 = &t;
	}
	if (cp)
		r = __syscall_cp5(SYS__umtx_op, addr, op, (unsigned)val, a1, a2);
	else
		r = __syscall5(SYS__umtx_op, addr, op, (unsigned)val, a1, a2);
	if (r == -ETIMEDOUT || r == -EINTR || r == -ECANCELED || r == -EINVAL)
		return (int)-r;
	return 0;
}

int __timedwait(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv)
{
	return fwait(addr, val, clk, at, priv, 0);
}

int __timedwait_cp(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv)
{
	return fwait(addr, val, clk, at, priv, 1);
}

void __wait(volatile int *addr, int val, int priv)
{
	fwait(addr, val, 0, 0, priv, 0);
}

void __wake(volatile int *addr, int cnt, int priv)
{
	if (cnt < 0)
		cnt = 0x7fffffff;
	__umtx_op((void *)addr, priv ? UMTX_OP_WAKE_PRIVATE : UMTX_OP_WAKE, (unsigned)cnt, 0, 0);
}
