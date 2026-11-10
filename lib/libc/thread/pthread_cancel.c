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

/* pthread_cancel and the cancelability state (see pthread_impl.h). */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"
#include "lock.h"

extern const char __cp_begin[], __cp_end[], __cp_cancel[];

/* ucontext_t as longs: the program counter (gregs[REG_RIP]) and the
 * signal mask the kernel restores */
#define UC_RIP 21
#define UC_SIGMASK 37

static void cancel_handler(int sig, siginfo_t *si, void *ctx)
{
	struct __pthread *self = __pthread_self();
	unsigned long *uc = ctx, pc = uc[UC_RIP];
	(void)sig; (void)si;
	a_barrier();
	if (!self->cancel || self->canceldisable == 1)
		return;
	uc[UC_SIGMASK] |= 1UL << (SIGCANCEL - 1);
	if (self->cancelasync) {
		__restore_sigs(&uc[UC_SIGMASK]);
		__cancel();
	}
	if (pc >= (unsigned long)__cp_begin && pc < (unsigned long)__cp_end) {
		uc[UC_RIP] = (unsigned long)__cp_cancel;
		return;
	}
	/* not at a cancellation point: keep it pending (blocked) */
	__syscall2(SYS_tkill, self->tid, SIGCANCEL);
}

static volatile int installed, install_lock;

int pthread_cancel(__pthread_t t)
{
	struct __pthread *self = __pthread_self();
	if (!installed) {
		LOCK(&install_lock);
		if (!installed) {
			struct sigaction sa;
			memset(&sa, 0, sizeof sa);
			sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
			sa.sa_sigaction = cancel_handler;
			memset(&sa.sa_mask, -1, sizeof sa.sa_mask);
			__libc_sigaction(SIGCANCEL, &sa, 0);
			installed = 1;
		}
		UNLOCK(&install_lock);
	}
	a_store(&t->cancel, 1);
	if (t == self) {
		if (!self->canceldisable && self->cancelasync)
			__cancel();
		return 0;
	}
	return __pthread_kill(t, SIGCANCEL);
}

void pthread_testcancel(void)
{
	__testcancel();
}

int pthread_setcancelstate(int new, int *old)
{
	if ((unsigned)new > 1)
		return EINVAL;
	return __setcancelstate(new, old);
}

int pthread_setcanceltype(int new, int *old)
{
	struct __pthread *self = __pthread_self();
	if ((unsigned)new > 1)
		return EINVAL;
	if (old)
		*old = self->cancelasync;
	self->cancelasync = (unsigned char)new;
	if (new)
		__testcancel();
	return 0;
}
