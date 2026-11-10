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

#include <unistd.h>
#include <signal.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "clofork.h"
#include "libc.h"

void __malloc_atfork(int);
void (*__atfork_hook)(int);  /* set by pthread_atfork(): -1 prepare, 0 parent, 1 child */

/* _Fork: async-signal-safe; no handlers, no locking. */
pid_t _Fork(void)
{
	long r;
	unsigned long all = -1UL, old;
	/* block signals so that no handler runs in the child with stale state */
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &all, &old, 8);
	r = __syscall5(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (r == 0) {
		/* the child: the only thread */
		struct __pthread *self = __pthread_self();
		self->tid = (int)__syscall0(SYS_gettid);
		self->next = self->prev = self;
		self->robust_registered = 0;
		self->robust_list.head = &self->robust_list.head;
		self->robust_list.pending = 0;
		__libc.threads_minus_1 = 0;
		__thread_list_lock = 0;
		__clofork_close_all();
	}
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &old, 0, 8);
	return (pid_t)__syscall_ret((unsigned long)r);
}

pid_t fork(void)
{
	pid_t r;
	if (__atfork_hook) __atfork_hook(-1);
	__tl_lock();
	__malloc_atfork(-1);
	r = _Fork();
	__malloc_atfork(r == 0 ? 1 : 0);
	if (r) __tl_unlock();       /* the child's is reset */
	if (__atfork_hook) __atfork_hook(r == 0 ? 1 : 0);
	return r;
}
