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

/* Scheduling of threads (rtprio_thread), and their CPU-time clocks. */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "lock.h"

int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *p)
{
	__sm_t set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	r = t->tid ? (int)-__thr_setsched(t->tid, policy, p->sched_priority) : ESRCH;
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

int pthread_getschedparam(pthread_t t, int *__RESTRICT policy, struct sched_param *__RESTRICT p)
{
	__sm_t set;
	long r;
	int pol = 0, prio = 0;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	if (!t->tid)
		r = -ESRCH;
	else {
		r = __thr_getsched(t->tid, &pol, &prio);
		if (r >= 0) {
			memset(p, 0, sizeof *p);
			p->sched_priority = prio;
			*policy = pol;
			r = 0;
		}
	}
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return (int)-r;
}

int pthread_setschedprio(pthread_t t, int prio)
{
	__sm_t set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	if (!t->tid)
		r = ESRCH;
	else {
		int pol;
		long e = __thr_getsched(t->tid, &pol, &r);
		r = e < 0 ? (int)-e : (int)-__thr_setsched(t->tid, pol, prio);
	}
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

/* the kernel's clock of a thread (clock_getcpuclockid2, CPUCLOCK_WHICH_TID) */
int pthread_getcpuclockid(pthread_t t, clockid_t *c)
{
	long r = __syscall3(SYS_clock_getcpuclockid2, t->tid, 1, c);
	return (int)-r;
}
