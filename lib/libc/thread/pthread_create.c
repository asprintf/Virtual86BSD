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

/* pthread_create.  A new thread's memory is one mapping:
 *
 *   [guard][ stack ... ][ TSD array ][ TLS block | struct __pthread ]
 *                     ^ initial stack pointer
 *
 * (with a stack from pthread_attr_setstack(), only the TSD and TLS are
 * mapped).  The creator blocks all signals around clone(); the child
 * restores the creator's mask once it runs.  With explicit scheduling
 * the child waits (startlock) until the creator has set its policy. */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sched.h>
#include "pthread_impl.h"
#include "libc.h"
#include "syscall.h"
#include "atomic.h"

#define DEFAULT_STACK (2UL << 20)

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_THREAD         0x00010000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_DETACHED       0x00400000

typedef union { long __l[7]; void *__p[7]; } attr_t;   /* pthread_attr_t */

static int start(void *p)
{
	struct __pthread *self = p;
	if (self->startlock) {
		while (self->startlock == 1)
			__wait(&self->startlock, 1, 1);
		if (self->startlock == 3)
			__pthread_exit(0);
	}
	__restore_sigs(&self->sigmask);
	if (self->c11)
		__pthread_exit((void *)(long)((int (*)(void *))(void *)self->start)(self->start_arg));
	__pthread_exit(self->start(self->start_arg));
	return 0;
}

int __pthread_create(__pthread_t *res, const void *attrp, void *(*fn)(void *), void *arg)
{
	struct __pthread *self = __pthread_self(), *new;
	attr_t attr;
	size_t pg = __libc.page_size, tls = __libc.tls_area_size;
	size_t tsdsz = PTHREAD_KEYS_MAX * sizeof(void *), size, guard = 0, stacksize;
	unsigned char *map, *top, *stack;
	unsigned long set;
	long r;
	int explicit, sched_failed = 0;

	if (attrp)
		memcpy(&attr, attrp, sizeof attr);
	else {
		memset(&attr, 0, sizeof attr);
		attr._a_stacksize = DEFAULT_STACK;
		attr._a_guardsize = pg;
	}
	stacksize = attr._a_stacksize;
	explicit = attr._a_sched & 1;
	if (attr._a_stackaddr) {
		size = (tls + tsdsz + pg - 1) & -pg;
		r = __syscall6x(SYS_mmap, 0, size, 3, 0x22, -1, 0);
		if ((unsigned long)r > -4096UL)
			return EAGAIN;
		map = (unsigned char *)r;
		stack = (unsigned char *)attr._a_stackaddr + stacksize;
	} else {
		guard = (attr._a_guardsize + pg - 1) & -pg;
		size = guard + ((stacksize + tls + tsdsz + pg - 1) & -pg);
		r = __syscall6x(SYS_mmap, 0, size, 0, 0x22 | 0x20000 /* NORESERVE */, -1, 0);
		if ((unsigned long)r > -4096UL)
			return EAGAIN;
		map = (unsigned char *)r;
		if (__syscall3(SYS_mprotect, map + guard, size - guard, 3) < 0) {
			__syscall2(SYS_munmap, map, size);
			return EAGAIN;
		}
		stack = 0;
	}
	top = map + size;
	new = __copy_tls(top - tls);
	new->tsd = (void **)(top - tls - tsdsz);
	if (!stack)
		stack = (unsigned char *)new->tsd;
	stack = (unsigned char *)((unsigned long)stack & -16UL);
	new->map_base = map;
	new->map_size = size;
	new->stack = stack;
	new->stack_size = attr._a_stackaddr ? stacksize : (size_t)(stack - map - guard);
	new->guard_size = guard;
	new->detach_state = attr._a_detach ? DT_DETACHED : DT_JOINABLE;
	new->canary = self->canary;
	new->pointer_guard = self->pointer_guard;
	new->sysinfo = self->sysinfo;
	new->robust_list.head = &new->robust_list.head;
	new->start = fn;
	new->start_arg = arg;
	new->c11 = (attr._a_flags & 2) != 0;
	new->startlock = explicit;

	__block_all_sigs(&set);
	new->sigmask = set;
	__tl_lock();
	if (!__libc.threaded)
		__libc.threaded = 1;
	r = __clone(start, stack, CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
	            CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_DETACHED,
	            new, &new->tid, new, &new->detach_state);
	if (r >= 0) {
		new->next = self->next;
		new->prev = self;
		self->next->prev = new;
		self->next = new;
		__libc.threads_minus_1++;
		if (explicit) {
			/* (the packing: pthread_attr.c) */
			struct sched_param sp = {0};
			int pol = (int)((attr._a_sched >> 8) & 0xff);
			unsigned long pr = (unsigned long)attr._a_prio;
			sp.sched_priority = (int)(unsigned)pr;
			if (pol == SCHED_SPORADIC) {
				unsigned long b = (unsigned long)attr._a_sched >> 16, rp = (unsigned long)attr._a_flags >> 2;
				sp.sched_ss_low_priority = (int)(pr >> 32 & 0xff);
				sp.sched_ss_max_repl = (int)(pr >> 40 & 0xff);
				sp.sched_ss_init_budget.tv_sec = (time_t)(b / 1000000);
				sp.sched_ss_init_budget.tv_nsec = (long)(b % 1000000 * 1000);
				sp.sched_ss_repl_period.tv_sec = (time_t)(rp / 1000000);
				sp.sched_ss_repl_period.tv_nsec = (long)(rp % 1000000 * 1000);
			}
			long e = __syscall3(SYS_sched_setscheduler, new->tid, pol, &sp);
			if (e < 0) {
				new->detach_state = DT_DETACHED;   /* it unmaps itself */
				r = e;
				sched_failed = 1;
			}
			a_store(&new->startlock, e < 0 ? 3 : 2);
			__wake(&new->startlock, 1, 1);
		}
	}
	__tl_unlock();
	__restore_sigs(&set);
	if (r < 0) {
		if (!sched_failed)          /* clone failed: nothing runs there */
			__syscall2(SYS_munmap, map, size);
		return r == -EPERM ? EPERM : r == -EINVAL ? EINVAL : EAGAIN;
	}
	*res = new;
	return 0;
}

extern __typeof(__pthread_create) pthread_create __attribute__((weak, alias("__pthread_create")));
