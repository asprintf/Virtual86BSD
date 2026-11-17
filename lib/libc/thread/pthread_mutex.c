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

/* Mutexes, on the kernel's userland mutex (struct umutex, _umtx_op).  The lock word
 * is the owner's thread id, with UMUTEX_CONTESTED while others sleep on it; an
 * uncontested lock and unlock are one compare-and-swap, the rest is the kernel's:
 * waiting (also with a timeout), PTHREAD_PRIO_INHERIT (the owner is lent the
 * priority of the waiters), PTHREAD_PRIO_PROTECT (the owner runs at the ceiling) and
 * robust mutexes: the robust ones held are on a list of the thread (the link is in the
 * umutex) that the kernel walks when the thread ends, in whatever way, marking them
 * UMUTEX_RB_OWNERDEAD, so the next lock returns EOWNERDEAD.  A robust mutex that is
 * unlocked while it is not made consistent is UMUTEX_RB_NOTRECOV: ENOTRECOVERABLE. */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <stdint.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"
#include "umtx.h"

#define OWNER(v) ((int)((v) & ~UMUTEX_CONTESTED))
#define RTP_PRIO_MAX 31

struct rb_params { uintptr_t list, priv, inact; };

/* tell the kernel where the thread's robust lists are */
int __robust_register(struct __pthread *self)
{
	if (!self->robust_registered) {
		struct rb_params p = { (uintptr_t)&self->robust_list.head, (uintptr_t)&self->robust_list.priv,
		    (uintptr_t)&self->robust_list.pending };
		self->robust_list.head = self->robust_list.priv = self->robust_list.pending = 0;
		if (__umtx_op(0, UMTX_OP_ROBUST_LISTS, sizeof p, &p, 0) < 0)
			return ENOMEM;
		self->robust_registered = 1;
	}
	return 0;
}

/* (the kernel does the work for a thread that ends) */
void __robust_exit(struct __pthread *self) { (void)self; }

static unsigned kflags(unsigned type)
{
	unsigned f = 0;
	if (type & __M_SHARED) f |= UMUTEX_SHARED;
	if (type & __M_ROBUST) f |= UMUTEX_ROBUST;
	if (type & __M_PI) f |= UMUTEX_PRIO_INHERIT;
	if (type & __M_PP) f |= UMUTEX_PRIO_PROTECT;
	return f;
}

/* attributes (pthread_mutexattr.c): type 0-1, robust 4, protocol << 8,
 * process-shared 128, ceiling << 16 */
int __pthread_mutex_init(pthread_mutex_t *__RESTRICT m, const pthread_mutexattr_t *__RESTRICT a)
{
	memset(m, 0, sizeof *m);
	if (a) {
		unsigned t = a->__attr & (3 | __M_ROBUST | __M_SHARED), proto = (a->__attr >> 8) & 3;
		if (proto == PTHREAD_PRIO_INHERIT)
			t |= __M_PI;
		else if (proto == PTHREAD_PRIO_PROTECT)
			t |= __M_PP | __M_CEIL(a->__attr) << 16;
		m->_m_type = (int)t;
		m->_m_kflags = (int)kflags(t);
		m->_m_kceil = (int)__M_CEIL(t);
		if (t & __M_PP)        /* the kernel's unlocked PP mutex (do_lock_pp, do_set_ceiling) */
			m->_m_lock = (int)UMUTEX_CONTESTED;
	}
	return 0;
}

int __pthread_mutex_destroy(pthread_mutex_t *m)
{
	(void)m;
	return 0;
}

/* the robust mutexes held, newest first, linked through the umutexes */
static void robust_add(struct __pthread *self, pthread_mutex_t *m)
{
	m->_m_rb = (void *)self->robust_list.head;
	self->robust_list.head = m;
}

static void robust_remove(struct __pthread *self, pthread_mutex_t *m)
{
	void *volatile *p = (void *volatile *)&self->robust_list.head;
	while (*p && *p != (void *)m)
		p = &((pthread_mutex_t *)*p)->_m_rb;
	if (*p)
		*p = m->_m_rb;
	m->_m_rb = 0;
}

/* the kernel's lock: 0, EOWNERDEAD, or an error number */
static int klock(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct _umtx_time t;
	void *a1 = 0, *a2 = 0;
	long r;
	if (at) {
		if ((unsigned long)at->tv_nsec >= 1000000000UL)
			return EINVAL;
		if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
			return EINVAL;
		if (at->tv_sec < 0)
			return ETIMEDOUT;
		t._timeout = *at;
		t._flags = UMTX_ABSTIME;
		t._clockid = (unsigned)clk;
		a1 = (void *)sizeof t;
		a2 = &t;
	}
	do
		r = __umtx_op(m, try ? UMTX_OP_MUTEX_TRYLOCK : UMTX_OP_MUTEX_LOCK, 0, a1, a2);
	while (r == -EINTR);
	return (int)-r;
}

/* a normal mutex relocked by its owner: sleep until the timeout, or for ever */
static int selflock(clockid_t clk, const struct timespec *at)
{
	struct timespec forever = {1000000, 0};
	long r;
	if (at && (unsigned long)at->tv_nsec >= 1000000000UL)
		return EINVAL;
	for (;;) {
		if (!at) {
			__syscall2(SYS_nanosleep, &forever, 0);
			continue;
		}
		/* clock_nanosleep returns the error number itself (kern_posix_error) */
		r = __syscall4(SYS_clock_nanosleep, clk, TIMER_ABSTIME, at, 0);
		if (r != EINTR)
			return r ? (int)(r < 0 ? -r : r) : ETIMEDOUT;
	}
}

static int lock(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct __pthread *self = __pthread_self();
	int tid = self->tid, type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST;
	unsigned v = (unsigned)m->_m_lock;
	int r;

	if (OWNER(v) == tid) {
		if (kind == PTHREAD_MUTEX_RECURSIVE) {
			if ((unsigned)m->_m_count >= INT_MAX)
				return EAGAIN;
			m->_m_count++;
			return 0;
		}
		/* trylock: locked, "including by the current thread" */
		if (try)
			return EBUSY;
		if (kind == PTHREAD_MUTEX_ERRORCHECK || robust)
			return EDEADLK;
		/* a normal mutex: deadlock, as POSIX requires (the kernel's PI and PP locks say EDEADLK) */
		if (type & (__M_PI | __M_PP))
			return selflock(clk, at);
	}
	if (robust) {
		if ((r = __robust_register(self)))
			return r;
		self->robust_list.pending = m;     /* if the thread ends before it is in the list */
	}
	if (!(type & __M_PP) && !a_cas(&m->_m_lock, 0, tid))
		r = 0;
	else
		r = klock(m, clk, at, try);
	if (r == 0 || r == EOWNERDEAD) {
		if (kind == PTHREAD_MUTEX_RECURSIVE)
			m->_m_count = 0;
		if (type & __M_PP) {
			m->_m_ppnext = self->pp_top;
			self->pp_top = m;
		}
		if (robust) {
			robust_add(self, m);
			if (r == EOWNERDEAD)
				a_or(&m->_m_kflags, UMUTEX_NONCONSISTENT);
		}
	}
	if (robust)
		self->robust_list.pending = 0;
	return r;
}

int __pthread_mutex_lock(pthread_mutex_t *m)
{
	return lock(m, CLOCK_REALTIME, 0, 0);
}

int __pthread_mutex_trylock(pthread_mutex_t *m)
{
	return lock(m, CLOCK_REALTIME, 0, 1);
}

int __pthread_mutex_clocklock(pthread_mutex_t *__RESTRICT m, clockid_t clk, const struct timespec *__RESTRICT at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
		return EINVAL;
	return lock(m, clk, at, 0);
}

int __pthread_mutex_timedlock(pthread_mutex_t *__RESTRICT m, const struct timespec *__RESTRICT at)
{
	return __pthread_mutex_clocklock(m, CLOCK_REALTIME, at);
}

/* off the owner's PP list; the kernel's unlock then lends the ceiling of
 * the PP mutex locked before it (m_ceilings[1]), or none (-1) */
static void pp_remove(struct __pthread *self, pthread_mutex_t *m)
{
	void **p = &self->pp_top;
	while (*p && *p != m)
		p = &((pthread_mutex_t *)*p)->_m_ppnext;
	if (*p)
		*p = m->_m_ppnext;
	m->_m_ppnext = 0;
	m->_m_kceil1 = self->pp_top ? ((pthread_mutex_t *)self->pp_top)->_m_kceil : -1;
}

int __pthread_mutex_unlock(pthread_mutex_t *m)
{
	struct __pthread *self = __pthread_self();
	int type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST, tid = self->tid;
	unsigned v = (unsigned)m->_m_lock;
	long r = 0;

	if (kind != PTHREAD_MUTEX_NORMAL || robust || (type & (__M_PI | __M_PP))) {
		if (OWNER(v) != tid)
			return EPERM;
		if (kind == PTHREAD_MUTEX_RECURSIVE && m->_m_count) {
			m->_m_count--;
			return 0;
		}
	}
	if (robust) {
		self->robust_list.pending = m;
		robust_remove(self, m);
	}
	if (type & __M_PP)
		pp_remove(self, m);
	/* a robust mutex left inconsistent, and the protocols, are the kernel's to unlock */
	if ((type & __M_PP) || (m->_m_kflags & UMUTEX_NONCONSISTENT) || a_cas(&m->_m_lock, tid, 0) != tid)
		r = __umtx_op(m, UMTX_OP_MUTEX_UNLOCK, 0, 0, 0);
	if (robust)
		self->robust_list.pending = 0;
	return (int)-r;
}

/* pthread_mutex_setprioceiling: the kernel takes the mutex, sets the ceiling, gives it back */
int __pthread_mutex_setceiling(pthread_mutex_t *m, int prio, int *old)
{
	int type = m->_m_type;
	unsigned oldc = 0;
	long r;
	if (!(type & __M_PP) || prio < 0 || prio > RTP_PRIO_MAX)
		return EINVAL;
	r = __umtx_op(m, UMTX_OP_SET_CEILING, (unsigned)prio, &oldc, 0);
	if (r < 0)
		return (int)-r;
	if (old)
		*old = (int)oldc;
	m->_m_type = (m->_m_type & ~(255 << 16)) | prio << 16;
	return 0;
}

extern __typeof(__pthread_mutex_init) pthread_mutex_init __attribute__((weak, alias("__pthread_mutex_init")));
extern __typeof(__pthread_mutex_destroy) pthread_mutex_destroy __attribute__((weak, alias("__pthread_mutex_destroy")));
extern __typeof(__pthread_mutex_lock) pthread_mutex_lock __attribute__((weak, alias("__pthread_mutex_lock")));
extern __typeof(__pthread_mutex_trylock) pthread_mutex_trylock __attribute__((weak, alias("__pthread_mutex_trylock")));
extern __typeof(__pthread_mutex_timedlock) pthread_mutex_timedlock __attribute__((weak, alias("__pthread_mutex_timedlock")));
extern __typeof(__pthread_mutex_clocklock) pthread_mutex_clocklock __attribute__((weak, alias("__pthread_mutex_clocklock")));
extern __typeof(__pthread_mutex_unlock) pthread_mutex_unlock __attribute__((weak, alias("__pthread_mutex_unlock")));
