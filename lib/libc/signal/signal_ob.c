/*
 * Copyright (c) 2026, asprintf
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

/* bsd_signal (SUSv2, SUSv3), sigstack (SUSv2) */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <signal.h>

void (*bsd_signal(int sig, void (*func)(int)))(int)
{
	struct sigaction act, oact;
	act.sa_handler = func;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, sig);
	if (sigaction(sig, &act, &oact) == -1)
		return SIG_ERR;
	return oact.sa_handler;
}

/* ss_onstack 0 disables the stack (4.3BSD differed); ss_sp is the top of a
 * SIGSTKSZ stack. */
int sigstack(struct sigstack *ss, struct sigstack *oss)
{
	stack_t n, o;
	if (ss) {
		if (ss->ss_onstack) {
			n.ss_sp = (char *)ss->ss_sp - SIGSTKSZ;
			n.ss_size = SIGSTKSZ;
			n.ss_flags = 0;
		} else {
			n.ss_sp = 0;
			n.ss_size = 0;
			n.ss_flags = SS_DISABLE;
		}
	}
	if (sigaltstack(ss ? &n : 0, &o) < 0)
		return -1;
	if (oss) {
		oss->ss_onstack = (o.ss_flags & SS_ONSTACK) != 0;
		oss->ss_sp = (o.ss_flags & SS_DISABLE) ? 0 : (char *)o.ss_sp + o.ss_size;
	}
	return 0;
}
