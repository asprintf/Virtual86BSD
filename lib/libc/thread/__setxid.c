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

/* set*id in a process with threads: Linux keeps credentials per
 * thread, POSIX per process.  The caller changes its own, then has every
 * other thread repeat the call from a SIGSETXID handler, under the
 * thread-list lock (so no thread starts or ends meanwhile).  A thread
 * whose call fails while the caller's succeeded would leave the process
 * with mixed credentials: the process is then killed. */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include "pthread_impl.h"
#include "libc.h"
#include "syscall.h"
#include "atomic.h"
#include "lock.h"

static long xnr, xa, xb, xc;
static volatile int pending, failed, installed;

static void handler(int sig)
{
	(void)sig;
	if (__syscall3(xnr, xa, xb, xc) < 0)
		failed = 1;
	if (a_fetch_add(&pending, -1) == 1)
		__wake(&pending, 1, 1);
}

long __setxid(long nr, long a, long b, long c)
{
	struct __pthread *self = __pthread_self(), *t;
	long r;
	int n;
	if (!__libc.threads_minus_1)
		return __syscall_ret((unsigned long)__syscall3(nr, a, b, c));
	__tl_lock();
	if (!installed) {
		struct sigaction sa;
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = handler;
		sa.sa_flags = SA_RESTART;
		memset(&sa.sa_mask, -1, sizeof sa.sa_mask);
		__libc_sigaction(SIGSETXID, &sa, 0);
		installed = 1;
	}
	r = __syscall3(nr, a, b, c);
	if (r >= 0 && self->next != self) {
		xnr = nr; xa = a; xb = b; xc = c;
		failed = 0;
		pending = __libc.threads_minus_1;
		for (t = self->next; t != self; t = t->next)
			__syscall2(SYS_tkill, t->tid, SIGSETXID);
		while ((n = pending))
			__wait(&pending, n, 1);
		if (failed)
			__syscall1(SYS_exit_group, 127);
	}
	__tl_unlock();
	return __syscall_ret((unsigned long)r);
}

/* The nice value of the process (setpriority PRIO_PROCESS of itself,
 * nice()): Linux keeps one per thread, POSIX one per process, so every
 * thread gets it.  The calling thread first, for the error; the others
 * cannot fail then (all share the credentials and limits). */
long __setnice(int prio)
{
	struct __pthread *self = __pthread_self(), *t;
	long r;
	if (!__libc.threads_minus_1)
		return __syscall3(SYS_setpriority, PRIO_PROCESS, self->tid, prio);
	__tl_lock();
	r = __syscall3(SYS_setpriority, PRIO_PROCESS, self->tid, prio);
	if (r >= 0)
		for (t = self->next; t != self; t = t->next)
			__syscall3(SYS_setpriority, PRIO_PROCESS, t->tid, prio);
	__tl_unlock();
	return r;
}
