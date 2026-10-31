/* Mutexes.  The lock word holds the owner's thread ID, with
 * FUTEX_WAITERS set while others may sleep on it; that is also the
 * protocol of the kernel's robust futexes, so robust mutexes need only
 * the per-thread list of the robust mutexes held (set_robust_list): when
 * a thread dies the kernel marks those FUTEX_OWNER_DIED and wakes a
 * waiter, whose lock then returns EOWNERDEAD.  Priority inheritance and
 * protection are not supported. */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"

#define LK(m) (&(m)->__vi[1])
#define TID_MASK 0x3fffffff
/* the robust list links the _m_next fields; the futex is 20 bytes before */
#define ROBUST_OFF (-20L)
#define ENTRY(m) ((void *)&(m)->_m_next)
#define MUTEX_OF(e) ((pthread_mutex_t *)((char *)(e) - 24))

int __robust_register(struct __pthread *self)
{
	if (!self->robust_registered) {
		self->robust_list.head = &self->robust_list.head;
		self->robust_list.off = ROBUST_OFF;
		self->robust_list.pending = 0;
		__syscall2(SYS_set_robust_list, &self->robust_list, 3 * sizeof(long));
		self->robust_registered = 1;
	}
	return 0;
}

int __pthread_mutex_init(pthread_mutex_t *__RESTRICT m, const pthread_mutexattr_t *__RESTRICT a)
{
	memset(m, 0, sizeof *m);
	if (a)
		m->_m_type = (int)(a->__attr & (3 | __M_ROBUST | __M_SHARED));
	return 0;
}

int __pthread_mutex_destroy(pthread_mutex_t *m)
{
	(void)m;
	return 0;
}

static int lock(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct __pthread *self = __pthread_self();
	int tid = self->tid, type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST;
	int priv = !(type & __M_SHARED), waited = 0, spins = 0, r;
	volatile int *l = LK(m);
	unsigned v;

	v = (unsigned)*l;
	if ((int)(v & TID_MASK) == tid && !(v == __M_NOTRECOVERABLE)) {
		if (kind == PTHREAD_MUTEX_RECURSIVE) {
			if ((unsigned)m->_m_count >= INT_MAX)
				return EAGAIN;
			m->_m_count++;
			return 0;
		}
		if (kind == PTHREAD_MUTEX_ERRORCHECK || robust)
			return EDEADLK;
		/* a normal mutex: deadlock, as POSIX requires */
	}
	if (robust) {
		__robust_register(self);
		self->robust_list.pending = ENTRY(m);
	}
	for (;;) {
		v = (unsigned)*l;
		if (robust && v == __M_NOTRECOVERABLE) {
			self->robust_list.pending = 0;
			return ENOTRECOVERABLE;
		}
		if (!(v & TID_MASK)) {
			/* free (or its owner died): take it, keeping the waiters'
			 * mark if we may have taken it from under them */
			unsigned want = (unsigned)tid | (v & FUTEX_WAITERS) | (waited ? FUTEX_WAITERS : 0);
			if ((unsigned)a_cas(l, (int)v, (int)want) != v)
				continue;
			break;
		}
		if (try) {
			if (robust)
				self->robust_list.pending = 0;
			return EBUSY;
		}
		if (spins < 100 && !(v & FUTEX_WAITERS)) {
			spins++;
			a_spin();
			continue;
		}
		if (!(v & FUTEX_WAITERS) && (unsigned)a_cas(l, (int)v, (int)(v | FUTEX_WAITERS)) != v)
			continue;
		r = __timedwait(l, (int)(v | FUTEX_WAITERS), clk, at, priv);
		waited = 1;
		if (r == ETIMEDOUT || r == EINVAL) {
			if (robust)
				self->robust_list.pending = 0;
			return r;
		}
	}
	if (kind == PTHREAD_MUTEX_RECURSIVE)
		m->_m_count = 0;
	if (robust) {
		/* insert at the head of the robust list */
		void **head = (void **)&self->robust_list.head, *first = *head;
		m->_m_next = first;
		m->_m_prev = head;
		if (first != (void *)head)
			MUTEX_OF(first)->_m_prev = ENTRY(m);
		*head = ENTRY(m);
		self->robust_list.pending = 0;
		if (v & FUTEX_OWNER_DIED) {
			m->_m_type |= __M_INCONSISTENT;
			return EOWNERDEAD;
		}
	}
	return 0;
}

int __pthread_mutex_lock(pthread_mutex_t *m)
{
	if (!(m->_m_type & ~__M_SHARED) && !a_cas(LK(m), 0, __pthread_self()->tid))
		return 0;
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
	if (!(m->_m_type & ~__M_SHARED) && !a_cas(LK(m), 0, __pthread_self()->tid))
		return 0;
	return lock(m, clk, at, 0);
}

int __pthread_mutex_timedlock(pthread_mutex_t *__RESTRICT m, const struct timespec *__RESTRICT at)
{
	return __pthread_mutex_clocklock(m, CLOCK_REALTIME, at);
}

int __pthread_mutex_unlock(pthread_mutex_t *m)
{
	struct __pthread *self = __pthread_self();
	int type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST, priv = !(type & __M_SHARED);
	volatile int *l = LK(m);
	unsigned v = (unsigned)*l;

	if (kind != PTHREAD_MUTEX_NORMAL || robust) {
		if ((int)(v & TID_MASK) != self->tid || v == __M_NOTRECOVERABLE)
			return EPERM;
		if (kind == PTHREAD_MUTEX_RECURSIVE && m->_m_count) {
			m->_m_count--;
			return 0;
		}
	}
	if (robust) {
		void **prev = m->_m_prev, *next = m->_m_next;
		int nr = (type & __M_INCONSISTENT) != 0;
		self->robust_list.pending = ENTRY(m);
		*prev = next;
		if (next != (void *)&self->robust_list.head)
			MUTEX_OF(next)->_m_prev = prev;
		if (nr)
			m->_m_type &= ~__M_INCONSISTENT;
		v = (unsigned)a_swap(l, nr ? __M_NOTRECOVERABLE : 0);
		self->robust_list.pending = 0;
		if (nr) {
			__wake(l, -1, priv);
			return 0;
		}
	} else
		v = (unsigned)a_swap(l, 0);
	if (v & FUTEX_WAITERS)
		__wake(l, 1, priv);
	return 0;
}

extern __typeof(__pthread_mutex_init) pthread_mutex_init __attribute__((weak, alias("__pthread_mutex_init")));
extern __typeof(__pthread_mutex_destroy) pthread_mutex_destroy __attribute__((weak, alias("__pthread_mutex_destroy")));
extern __typeof(__pthread_mutex_lock) pthread_mutex_lock __attribute__((weak, alias("__pthread_mutex_lock")));
extern __typeof(__pthread_mutex_trylock) pthread_mutex_trylock __attribute__((weak, alias("__pthread_mutex_trylock")));
extern __typeof(__pthread_mutex_timedlock) pthread_mutex_timedlock __attribute__((weak, alias("__pthread_mutex_timedlock")));
extern __typeof(__pthread_mutex_clocklock) pthread_mutex_clocklock __attribute__((weak, alias("__pthread_mutex_clocklock")));
extern __typeof(__pthread_mutex_unlock) pthread_mutex_unlock __attribute__((weak, alias("__pthread_mutex_unlock")));
