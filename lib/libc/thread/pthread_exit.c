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

/* pthread_exit, and the cleanup handlers.  Only reserved names are
 * defined strongly: cancellation reaches here from the C library's own
 * cancellation points. */
#include <stdlib.h>
#include "pthread_impl.h"
#include "libc.h"
#include "lock.h"
#include "syscall.h"
#include "atomic.h"

struct __ptcb {
	void (*f)(void *);
	void *x;
	struct __ptcb *next;
};

/* the destructors of thread-specific data, if pthread_key_create (or
 * tss_create) is linked in */
extern void __pthread_tsd_run_dtors(void) __attribute__((weak));

_Noreturn void __pthread_exit(void *result)
{
	struct __pthread *self = __pthread_self();
	unsigned long set;
	int state;

	self->canceldisable = 1;
	self->cancelasync = 0;
	self->result = result;
	while (self->cancelbuf) {
		struct __ptcb *c = self->cancelbuf;
		self->cancelbuf = c->next;
		c->f(c->x);
	}
	if (__pthread_tsd_run_dtors)
		__pthread_tsd_run_dtors();

	/* signals stay deliverable while waiting for the lock (set*id
	 * broadcasts hold it while they wait for every listed thread) */
	__tl_lock();
	if (!__libc.threads_minus_1) {      /* the last thread: the process ends */
		__tl_unlock();
		exit(0);
	}
	__block_all_sigs(&set);
	LOCK(&self->killlock);
	self->prev->next = self->next;
	self->next->prev = self->prev;
	self->next = self->prev = self;
	__libc.threads_minus_1--;
	self->tid = 0;
	UNLOCK(&self->killlock);
	state = a_cas(&self->detach_state, DT_JOINABLE, DT_EXITING);
	__tl_unlock();
	if (__libc.tls_free)   /* dlopen'ed modules' TLS of this thread */
		__libc.tls_free(self);
	if (state == DT_DETACHED && self->map_base) {
		__syscall1(SYS_set_tid_address, 0);
		__unmapself(self->map_base, self->map_size);
	}
	for (;;)
		__syscall1(SYS_exit, 0);
}

extern __typeof(__pthread_exit) pthread_exit __attribute__((weak, alias("__pthread_exit")));

void __pthread_cleanup_push(struct __ptcb *c, void (*f)(void *), void *x)
{
	struct __pthread *self = __pthread_self();
	c->f = f;
	c->x = x;
	c->next = self->cancelbuf;
	self->cancelbuf = c;
}

void __pthread_cleanup_pop(struct __ptcb *c, int run)
{
	__pthread_self()->cancelbuf = c->next;
	if (run)
		c->f(c->x);
}
