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

#include <signal.h>
#include "syscall.h"

int raise(int sig)
{
	__sm_t all = { { -1UL, -1UL } }, old;
	long r;
	/* block everything so the signal is delivered on return from the
	 * mask restore, i.e. before raise() returns */
	__syscall3(SYS_sigprocmask, SIG_BLOCK, &all, &old);
	/* thr_kill queues every instance, even of a signal that is not queued
	 * (not real-time): one that is pending already is not sent again */
	if (sig > 0 && sig < 65) {
		__sm_t pend;
		if (__syscall1(SYS_sigpending, &pend) == 0 && (pend.__w[(sig - 1) / 64] >> ((sig - 1) % 64) & 1)) {
			__syscall3(SYS_sigprocmask, SIG_SETMASK, &old, 0);
			return 0;
		}
	}
	r = __syscall2(SYS_thr_kill, __thr_self(), sig);
	__syscall3(SYS_sigprocmask, SIG_SETMASK, &old, 0);
	return (int)__syscall_ret((unsigned long)r);
}
