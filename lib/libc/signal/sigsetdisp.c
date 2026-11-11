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

/* sigset(): SUSv2, SUSv3, SUSv4, removed in POSIX.1-2024 (sigaction()).
 * (signal/sigset.c is sigemptyset() and the other signal set functions.) */
#define _DEFAULT_SOURCE
#include <signal.h>
#include <string.h>

/* A handler runs with sig blocked, and the mask is restored after it
 * (sigaction() without SA_NODEFER and SA_RESTART).  SIG_HOLD blocks sig
 * and leaves the disposition; any other disposition unblocks it.  The
 * value is SIG_HOLD if sig was blocked, else the old disposition.  EINVAL
 * for a signal that cannot be caught or ignored comes from sigaction(). */
void (*sigset(int sig, void (*disp)(int)))(int)
{
	sigset_t mask, omask;
	struct sigaction sa, osa;

	sigemptyset(&mask);
	if (sigaddset(&mask, sig) < 0)
		return SIG_ERR;
	if (disp == SIG_HOLD) {
		if (sigprocmask(SIG_BLOCK, &mask, &omask) < 0 ||
		    sigaction(sig, 0, &osa) < 0)
			return SIG_ERR;
	} else {
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = disp;
		if (sigaction(sig, &sa, &osa) < 0 ||
		    sigprocmask(SIG_UNBLOCK, &mask, &omask) < 0)
			return SIG_ERR;
	}
	return sigismember(&omask, sig) ? SIG_HOLD : osa.sa_handler;
}
