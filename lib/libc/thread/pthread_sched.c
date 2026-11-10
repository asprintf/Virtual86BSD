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

/* Scheduling of threads, and their CPU-time clocks. */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "lock.h"

int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *p)
{
	unsigned long set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	r = t->tid ? (int)-__syscall3(SYS_sched_setscheduler, t->tid, policy, p) : ESRCH;
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

int pthread_getschedparam(pthread_t t, int *__RESTRICT policy, struct sched_param *__RESTRICT p)
{
	unsigned long set;
	long r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	if (!t->tid)
		r = -ESRCH;
	else if (memset(p, 0, sizeof *p), (r = __syscall2(SYS_sched_getparam, t->tid, p)) >= 0) {
		r = __syscall1(SYS_sched_getscheduler, t->tid);
		if (r >= 0) {
			*policy = (int)r;
			r = 0;
		}
	}
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return (int)-r;
}

int pthread_setschedprio(pthread_t t, int prio)
{
	unsigned long set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	if (!t->tid)
		r = ESRCH;
	else {     /* the whole struct: a sporadic server's other members stay */
		struct sched_param sp = {0};
		r = (int)-__syscall2(SYS_sched_getparam, t->tid, &sp);
		sp.sched_priority = prio;
		if (!r)
			r = (int)-__syscall2(SYS_sched_setparam, t->tid, &sp);
	}
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

/* the kernel's per-thread scheduler clock: ~tid << 3 | PERTHREAD | SCHED */
int pthread_getcpuclockid(pthread_t t, clockid_t *c)
{
	*c = (clockid_t)(~(unsigned)t->tid << 3 | 6);
	return 0;
}
