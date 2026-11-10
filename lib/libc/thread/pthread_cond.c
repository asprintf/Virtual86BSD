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

/* Condition variables.
 *
 * Process-private: each waiter queues a record on its own stack and
 * sleeps on its own futex word; pthread_cond_signal() dequeues the
 * oldest and sets its word, so a signal always goes to a thread that was
 * waiting at the time.
 *
 * Process-shared (no shared memory for such records): a sequence number
 * that every signal advances, and every signal wakes all waiters (a
 * spurious wakeup, as POSIX allows).
 *
 * A signaled waiter never touches the condition variable again, and
 * pthread_cond_destroy() waits for process-shared waiters still leaving,
 * so destroying one right after a broadcast is safe.
 *
 * Waiting is a cancellation point; cancellation acts only after the
 * mutex is reacquired, and a signal consumed by a cancelled waiter is
 * passed on. */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "atomic.h"
#include "lock.h"

#define __C_SHARED 0x100
#define DESTROYING ((int)0x80000000)

struct waiter {
	struct waiter *prev, *next;
	volatile int state;      /* 0 waiting, 1 signaled */
};

int __pthread_mutex_lock(pthread_mutex_t *);
int __pthread_mutex_unlock(pthread_mutex_t *);

int __pthread_cond_init(pthread_cond_t *__RESTRICT c, const pthread_condattr_t *__RESTRICT a)
{
	memset(c, 0, sizeof *c);
	if (a)
		c->_c_flags = (int)(a->__attr & 0xff) | (a->__attr >> 31 ? __C_SHARED : 0);
	return 0;
}

int __pthread_cond_destroy(pthread_cond_t *c)
{
	/* Process-shared waiters still reading the condition variable after
	   a broadcast are waited for (bit 31 of the count asks the last one
	   to wake us).  Process-private waiters that were signaled never
	   touch it again. */
	int v;
	if (c->_c_flags & __C_SHARED)
		while ((v = c->_c_waiters) & 0x7fffffff) {
			if (!(v & DESTROYING) && a_cas(&c->__vi[7], v, v | DESTROYING) != v)
				continue;
			__wait(&c->__vi[7], v | DESTROYING, 0);
		}
	return 0;
}

static void unqueue(pthread_cond_t *c, struct waiter *w)
{
	if (w->prev) w->prev->next = w->next; else c->_c_head = w->next;
	if (w->next) w->next->prev = w->prev; else c->_c_tail = w->prev;
}

static void wake_waiter(struct waiter *w)
{
	volatile int *p = &w->state;
	a_store(p, 1);           /* w may be gone from here on */
	__wake(p, 1, 1);
}

int __pthread_cond_signal(pthread_cond_t *c)
{
	struct waiter *w;
	if (c->_c_flags & __C_SHARED) {
		a_inc(&c->__vi[6]);
		if (c->_c_waiters)
			__wake(&c->__vi[6], -1, 0);
		return 0;
	}
	if (!c->_c_head)
		return 0;
	LOCK(&c->__vi[4]);
	w = c->_c_head;
	if (w)
		unqueue(c, w);
	if (w)
		wake_waiter(w);
	UNLOCK(&c->__vi[4]);
	return 0;
}

int __pthread_cond_broadcast(pthread_cond_t *c)
{
	struct waiter *w, *next;
	if (c->_c_flags & __C_SHARED) {
		a_inc(&c->__vi[6]);
		if (c->_c_waiters)
			__wake(&c->__vi[6], -1, 0);
		return 0;
	}
	if (!c->_c_head)
		return 0;
	LOCK(&c->__vi[4]);
	w = c->_c_head;
	c->_c_head = c->_c_tail = 0;
	for (; w; w = next) {
		next = w->next;
		wake_waiter(w);
	}
	UNLOCK(&c->__vi[4]);
	return 0;
}

static int cond_wait(pthread_cond_t *__RESTRICT c, pthread_mutex_t *__RESTRICT m, clockid_t clk,
                     const struct timespec *__RESTRICT at)
{
	struct waiter w;
	int cs, e = 0, r, shared = c->_c_flags & __C_SHARED, consumed = 0, seq = 0;
	int kind = m->_m_type & 3;

	if ((kind != PTHREAD_MUTEX_NORMAL || (m->_m_type & __M_ROBUST)) &&
	    (m->__vi[1] & 0x3fffffff) != __pthread_self()->tid)
		return EPERM;
	if (at && (unsigned long)at->tv_nsec >= 1000000000UL)
		return EINVAL;
	__setcancelstate(__CANCEL_MASKED, &cs);
	if (cs == 1)
		__setcancelstate(1, 0);

	if (shared) {
		seq = c->_c_seq;
		a_inc(&c->__vi[7]);
	} else {
		w.next = 0;
		w.state = 0;
		LOCK(&c->__vi[4]);
		w.prev = c->_c_tail;
		if (w.prev) w.prev->next = &w; else c->_c_head = &w;
		c->_c_tail = &w;
		UNLOCK(&c->__vi[4]);
	}
	__pthread_mutex_unlock(m);

	if (shared) {
		do e = __timedwait_cp(&c->__vi[6], seq, clk, at, 0);
		while (c->_c_seq == seq && (e == 0 || e == EINTR));
		if (c->_c_seq != seq)
			e = 0;
		/* the last access to *c, which may be destroyed from here on */
		if (a_fetch_add(&c->__vi[7], -1) == (DESTROYING | 1))
			__wake(&c->__vi[7], 1, 0);
	} else {
		while (!w.state) {
			e = __timedwait_cp(&w.state, 0, clk, at, 1);
			if (e == ETIMEDOUT || e == ECANCELED || e == EINVAL)
				break;
		}
		/* A signaled waiter must not touch *c again: it may already be
		   destroyed (it is safe to destroy one right after a broadcast) */
		if (!w.state) {
			LOCK(&c->__vi[4]);
			if (!w.state)
				unqueue(c, &w);
			UNLOCK(&c->__vi[4]);
		}
		if (w.state) {
			consumed = e == ECANCELED;
			if (!consumed)
				e = 0;
		}
	}

	r = __pthread_mutex_lock(m);
	__setcancelstate(cs, 0);
	if (e == ECANCELED) {
		if (consumed)
			__pthread_cond_signal(c);
		__testcancel();
	}
	if (e == ETIMEDOUT || e == EINVAL)
		return e;
	return r;
}

int __pthread_cond_timedwait(pthread_cond_t *__RESTRICT c, pthread_mutex_t *__RESTRICT m,
                             const struct timespec *__RESTRICT at)
{
	return cond_wait(c, m, c->_c_flags & 0xff, at);
}

int __pthread_cond_wait(pthread_cond_t *__RESTRICT c, pthread_mutex_t *__RESTRICT m)
{
	return cond_wait(c, m, CLOCK_REALTIME, 0);
}

int __pthread_cond_clockwait(pthread_cond_t *__RESTRICT c, pthread_mutex_t *__RESTRICT m, clockid_t clk,
                             const struct timespec *__RESTRICT at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
		return EINVAL;
	return cond_wait(c, m, clk, at);
}

extern __typeof(__pthread_cond_init) pthread_cond_init __attribute__((weak, alias("__pthread_cond_init")));
extern __typeof(__pthread_cond_destroy) pthread_cond_destroy __attribute__((weak, alias("__pthread_cond_destroy")));
extern __typeof(__pthread_cond_signal) pthread_cond_signal __attribute__((weak, alias("__pthread_cond_signal")));
extern __typeof(__pthread_cond_broadcast) pthread_cond_broadcast __attribute__((weak, alias("__pthread_cond_broadcast")));
extern __typeof(__pthread_cond_wait) pthread_cond_wait __attribute__((weak, alias("__pthread_cond_wait")));
extern __typeof(__pthread_cond_timedwait) pthread_cond_timedwait __attribute__((weak, alias("__pthread_cond_timedwait")));
extern __typeof(__pthread_cond_clockwait) pthread_cond_clockwait __attribute__((weak, alias("__pthread_cond_clockwait")));
