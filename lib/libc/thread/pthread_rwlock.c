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

/* Read-write locks: the lock word counts readers, or is 0x7fffffff when
 * write-locked; bit 31 marks sleepers, which every last unlock wakes.
 * Readers are preferred (as glibc's default), except as Thread Execution
 * Scheduling requires: a reader running SCHED_FIFO or SCHED_RR does not
 * take the lock while a writer of higher or equal priority (also FIFO or
 * RR) is blocked on it.  Those writers are counted in __vi[4], and __vi[5]
 * is 1 + the highest of their priorities (0: none).  When one of them
 * stops waiting the priority is cleared and the others, all woken, put
 * theirs back before they sleep again: on one processor the woken threads
 * run in priority order, so the outcome is POSIX's; on several, a reader
 * woken at the same moment may get in before a writer it should yield to. */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "atomic.h"
#include "syscall.h"
#include <sched.h>

#define WR 0x7fffffff
#define WAITING ((int)0x80000000)
#define L(rw) (&(rw)->__vi[0])
#define PRIV(rw) (!(rw)->_rw_shared)
#define OWNER(rw) (&(rw)->__vi[3])  /* tid of the write owner, for EDEADLK */
#define WW(rw) (&(rw)->__vi[4])     /* real-time writers blocked */
#define WP(rw) (&(rw)->__vi[5])     /* 1 + the highest priority among them */

/* the calling thread's priority when it runs SCHED_FIFO or SCHED_RR, else -1 */
static int rt_prio(void)
{
	struct __pthread *self = __pthread_self();
	struct sched_param sp;
	long pol = __syscall1(SYS_sched_getscheduler, self->tid);
	if (pol < 0)
		return -1;
	pol &= ~0x40000000;	/* SCHED_RESET_ON_FORK */
	if (pol != SCHED_FIFO && pol != SCHED_RR)
		return -1;
	memset(&sp, 0, sizeof sp);
	if (__syscall2(SYS_sched_getparam, self->tid, &sp) < 0)
		return -1;
	return sp.sched_priority;
}

int pthread_rwlock_init(pthread_rwlock_t *__RESTRICT rw, const pthread_rwlockattr_t *__RESTRICT a)
{
	memset(rw, 0, sizeof *rw);
	if (a && a->__attr[0])
		rw->_rw_shared = 128;
	return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rw) { (void)rw; return 0; }

static int sleep_on(pthread_rwlock_t *rw, int v, clockid_t clk, const struct timespec *at)
{
	int r;
	if (!(v & WAITING) && a_cas(L(rw), v, v | WAITING) != v)
		return 0;   /* changed meanwhile: try again */
	a_inc(&rw->__vi[1]);
	r = __timedwait(L(rw), v | WAITING, clk, at, PRIV(rw));
	a_dec(&rw->__vi[1]);
	return r == ETIMEDOUT || r == EINVAL ? r : 0;
}

static int rdlock(pthread_rwlock_t *rw, clockid_t clk, const struct timespec *at, int try)
{
	int v, n, r, prio = -2;
	if (!try && *OWNER(rw) == __pthread_self()->tid)
		return EDEADLK;
	for (;;) {
		v = *L(rw);
		n = v & WR;
		if (n != WR && *WW(rw) > 0) {
			if (prio == -2)
				prio = rt_prio();
			if (prio >= 0 && *WP(rw) > prio)
				n = WR;	/* a writer of higher or equal priority waits: as if write-locked */
		}
		if (n != WR) {
			if (n == WR - 1)
				return EAGAIN;
			if (a_cas(L(rw), v, v + 1) == v)
				return 0;
			continue;
		}
		if (try)
			return EBUSY;
		if ((r = sleep_on(rw, v, clk, at)))
			return r;
	}
}

/* a real-time writer stops waiting: the priority is cleared, and the
 * writers still waiting put theirs back when they are woken (all are, by
 * the unlock that follows an acquisition, or here after a time-out) */
static void wr_unregister(pthread_rwlock_t *rw, int acquired)
{
	a_dec(WW(rw));
	*WP(rw) = 0;
	if (!acquired)
		__wake(L(rw), -1, PRIV(rw));
}

static int wrlock(pthread_rwlock_t *rw, clockid_t clk, const struct timespec *at, int try)
{
	int v, r, prio = -2, w;
	if (!try && *OWNER(rw) == __pthread_self()->tid)
		return EDEADLK;
	for (;;) {
		v = *L(rw);
		if (!(v & WR)) {
			if (a_cas(L(rw), v, v | WR) == v) {
				*OWNER(rw) = __pthread_self()->tid;
				if (prio >= 0)
					wr_unregister(rw, 1);
				return 0;
			}
			continue;
		}
		if (try)
			return EBUSY;
		if (prio == -2) {
			prio = rt_prio();
			if (prio >= 0)
				a_inc(WW(rw));
		}
		if (prio >= 0)
			while ((w = *WP(rw)) < prio + 1 && a_cas(WP(rw), w, prio + 1) != w)
				;
		if ((r = sleep_on(rw, v, clk, at))) {
			if (prio >= 0)
				wr_unregister(rw, 0);
			return r;
		}
	}
}

static int clockcheck(clockid_t clk)
{
	return clk == CLOCK_REALTIME || clk == CLOCK_MONOTONIC ? 0 : EINVAL;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rw) { return rdlock(rw, CLOCK_REALTIME, 0, 0); }
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw) { return rdlock(rw, CLOCK_REALTIME, 0, 1); }
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__RESTRICT rw, const struct timespec *__RESTRICT at)
{
	return rdlock(rw, CLOCK_REALTIME, at, 0);
}
int pthread_rwlock_clockrdlock(pthread_rwlock_t *__RESTRICT rw, clockid_t clk, const struct timespec *__RESTRICT at)
{
	return clockcheck(clk) ? EINVAL : rdlock(rw, clk, at, 0);
}
int pthread_rwlock_wrlock(pthread_rwlock_t *rw) { return wrlock(rw, CLOCK_REALTIME, 0, 0); }
int pthread_rwlock_trywrlock(pthread_rwlock_t *rw) { return wrlock(rw, CLOCK_REALTIME, 0, 1); }
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__RESTRICT rw, const struct timespec *__RESTRICT at)
{
	return wrlock(rw, CLOCK_REALTIME, at, 0);
}
int pthread_rwlock_clockwrlock(pthread_rwlock_t *__RESTRICT rw, clockid_t clk, const struct timespec *__RESTRICT at)
{
	return clockcheck(clk) ? EINVAL : wrlock(rw, clk, at, 0);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
	int v, n, new;
	do {
		v = *L(rw);
		n = v & WR;
		if (!n)
			return EPERM;
		new = n == WR || n == 1 ? 0 : v - 1;
		if (n == WR)
			*OWNER(rw) = 0;
	} while (a_cas(L(rw), v, new) != v);
	if (!new && ((v & WAITING) || rw->_rw_waiters))
		__wake(L(rw), -1, PRIV(rw));
	return 0;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *a) { a->__attr[0] = a->__attr[1] = 0; return 0; }
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a) { (void)a; return 0; }
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__RESTRICT a, int *__RESTRICT s) { *s = (int)a->__attr[0]; return 0; }
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->__attr[0] = (unsigned)s;
	return 0;
}
