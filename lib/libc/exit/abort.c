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
#include "syscall.h"
#include "atomic.h"

#define SIGABRT 6

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned long mask;
};

_Noreturn void abort(void)
{
	unsigned long all = -1UL, abrt = 1UL << (SIGABRT - 1);
	struct k_sigaction sa = { 0, 0, 0, 0 }; /* SIG_DFL */
	int tid;

	/* First give a SIGABRT handler its chance. */
	tid = (int)__syscall0(SYS_gettid);
	__syscall2(SYS_tkill, tid, SIGABRT);

	/* It returned (or the signal was blocked/ignored): force default. */
	__syscall4(SYS_rt_sigprocmask, 0 /*SIG_BLOCK*/, &all, 0, 8);
	__syscall4(SYS_rt_sigaction, SIGABRT, &sa, 0, 8);
	__syscall2(SYS_tkill, tid, SIGABRT);
	__syscall4(SYS_rt_sigprocmask, 1 /*SIG_UNBLOCK*/, &abrt, 0, 8);

	/* Should be unreachable. */
	a_crash();
	for (;;) __syscall1(SYS_exit_group, 127);
}
