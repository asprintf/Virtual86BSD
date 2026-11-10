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

/* Cancellation points (see pthread_impl.h).  Only reserved names here:
 * the C library's own functions (read() under fread(), say) come here. */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"

long __syscall_cp_asm(volatile int *, long, long, long, long, long, long, long);

/* act on a cancellation request: exit, or (masked) report ECANCELED */
long __cancel(void)
{
	struct __pthread *self = __pthread_self();
	if (self->canceldisable == 0 || self->cancelasync)
		__pthread_exit((void *)-1);   /* PTHREAD_CANCELED */
	self->canceldisable = 1;
	return -ECANCELED;
}

long __syscall_cp_c(long nr, long a, long b, long c, long d, long e, long f)
{
	struct __pthread *self = __pthread_self();
	int st = self->canceldisable;
	long r;
	if (st && (st == 1 || nr == SYS_close))
		return __syscall6x(nr, a, b, c, d, e, f);
	r = __syscall_cp_asm(&self->cancel, nr, a, b, c, d, e, f);
	if (r == -EINTR && nr != SYS_close && self->cancel && self->canceldisable != 1)
		r = __cancel();
	return r;
}

void __testcancel(void)
{
	struct __pthread *self = __pthread_self();
	if (self->cancel && !self->canceldisable)
		__cancel();
}

/* also accepts __CANCEL_MASKED */
int __setcancelstate(int new, int *old)
{
	struct __pthread *self = __pthread_self();
	if ((unsigned)new > 2)
		return EINVAL;
	if (old)
		*old = self->canceldisable;
	self->canceldisable = (unsigned char)new;
	return 0;
}
