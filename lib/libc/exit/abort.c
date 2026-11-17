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

#include <stdlib.h>
#include <signal.h>
#include "syscall.h"
#include "atomic.h"

_Noreturn void abort(void)
{
	__sm_t all = { { -1UL, -1UL } }, abrt = { { 1UL << (SIGABRT - 1), 0 } };
	struct sigaction sa = { { 0 }, 0, { { 0 } } }; /* SIG_DFL */
	long tid;

	/* First give a SIGABRT handler its chance. */
	tid = __thr_self();
	__syscall2(SYS_thr_kill, tid, SIGABRT);

	/* It returned (or the signal was blocked/ignored): force default. */
	__syscall3(SYS_sigprocmask, SIG_BLOCK, &all, 0);
	__syscall3(SYS_sigaction, SIGABRT, &sa, 0);
	__syscall2(SYS_thr_kill, tid, SIGABRT);
	__syscall3(SYS_sigprocmask, SIG_UNBLOCK, &abrt, 0);

	/* Should be unreachable. */
	a_crash();
	for (;;) __syscall1(SYS__exit, 127);
}
