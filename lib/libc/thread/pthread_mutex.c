/* Mutexes.  The lock word holds the owner's thread ID, with
 * FUTEX_WAITERS set while others may sleep on it; that is also the
 * protocol of the kernel's robust futexes, so robust mutexes need only
 * the per-thread list of the robust mutexes held (set_robust_list): when
 * a thread dies the kernel marks those FUTEX_OWNER_DIED and wakes a
 * waiter, whose lock then returns EOWNERDEAD.
 * PTHREAD_PRIO_INHERIT uses the kernel's PI futexes, whose lock word is the
 * same (owner tid | FUTEX_WAITERS): contended, the kernel queues the
 * waiters and lends the owner their priority.  PTHREAD_PRIO_PROTECT keeps
 * the ordinary lock word and raises the owner to the mutex's ceiling while
 * it holds it (pp_enter/pp_leave). */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <stdint.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"

#define LK(m) (&(m)->__vi[1])
#define TID_MASK 0x3fffffff
/* the robust list links the _m_next fields; the futex is 20 bytes before */
#define ROBUST_OFF (-20L)
#define ENTRY(m) ((void *)&(m)->_m_next)
/* entries of PI mutexes are marked by bit 0 of the pointer to them, which
 * tells the kernel how to hand them over when their owner dies */
#define MUTEX_OF(e) ((pthread_mutex_t *)(((uintptr_t)(e) & ~(uintptr_t)1) - 24))
#define TAGGED(m) ((void *)((char *)ENTRY(m) + !!((m)->_m_type & __M_PI)))

#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_LOCK_PI2 13
#define FUTEX_PRIVATE 128
#define FUTEX_CLOCK_REALTIME 256

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
			t |= __M_PP | (__M_CEIL(a->__attr) ? __M_CEIL(a->__attr) : 1) << 16;
		m->_m_type = (int)t;
	}
	return 0;
}

int __pthread_mutex_destroy(pthread_mutex_t *m)
{
	(void)m;
	return 0;
}

/* PTHREAD_PRIO_PROTECT: while the thread holds mutexes with ceilings, a
 * thread of SCHED_FIFO or SCHED_RR runs at the highest of them if that is
 * above its own priority.  Other policies have no priorities (Linux:
 * 0 only), so their threads are not raised (glibc fails the lock there). */
static int setprio(struct __pthread *self, int prio)
{
	struct sched_param sp;
	long pol = __syscall1(SYS_sched_getscheduler, self->tid);
	if (pol < 0)
		return (int)-pol;
	if (pol != SCHED_FIFO && pol != SCHED_RR)
		return 0;
	sp.sched_priority = prio;
	return (int)-__syscall3(SYS_sched_setscheduler, self->tid, pol, &sp);
}

static int own_prio(struct __pthread *self)
{
	struct sched_param sp;
	long pol;
	if (self->pp_max)
		return self->pp_base;
	pol = __syscall1(SYS_sched_getscheduler, self->tid);
	if (pol != SCHED_FIFO && pol != SCHED_RR)
		return 0;
	if (__syscall2(SYS_sched_getparam, self->tid, &sp) < 0)
		return 0;
	return sp.sched_priority;
}

/* pp_max: the highest ceiling held (0: none); pp_base: the thread's own
 * priority, noted when it took the first; it runs at the higher of both */
static int pp_enter(struct __pthread *self, int ceil)
{
	int r, base = own_prio(self);
	if (base > ceil)
		return EINVAL;
	if (self->pp_count[ceil] == 0xffff)
		return EAGAIN;
	if (ceil > self->pp_max) {
		int old = self->pp_max > base ? self->pp_max : base;
		if (ceil > old && (r = setprio(self, ceil)))
			return r;
		self->pp_base = base;
		self->pp_max = ceil;
	}
	self->pp_count[ceil]++;
	return 0;
}

static void pp_leave(struct __pthread *self, int ceil)
{
	int i;
	if (--self->pp_count[ceil] || ceil != self->pp_max)
		return;
	for (i = ceil - 1; i > 0 && !self->pp_count[i]; i--);
	self->pp_max = i;
	if (ceil > self->pp_base)
		setprio(self, i > self->pp_base ? i : self->pp_base);
}

/* insert m at the head of the thread's robust list */
static void robust_add(struct __pthread *self, pthread_mutex_t *m)
{
	void **head = (void **)&self->robust_list.head, *first = *head;
	m->_m_next = first;
	m->_m_prev = head;
	if (first != (void *)head)
		MUTEX_OF(first)->_m_prev = ENTRY(m);
	*head = TAGGED(m);
}

static int acquire(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try);
static int acquire_pi(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try);
static int unlock(pthread_mutex_t *m, int pp);

static int lock(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct __pthread *self = __pthread_self();
	int tid = self->tid, type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST;
	int r;
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
		/* trylock: locked, "including by the current thread" */
		if (try)
			return EBUSY;
		if (kind == PTHREAD_MUTEX_ERRORCHECK || robust)
			return EDEADLK;
		/* a normal mutex: deadlock, as POSIX requires */
	}
	if (type & __M_PI)
		return acquire_pi(m, clk, at, try);
	if (type & __M_PP) {
		int ceil = (int)__M_CEIL(type);
		if ((r = pp_enter(self, ceil)))
			return r;
		r = acquire(m, clk, at, try);
		if (r && r != EOWNERDEAD) {
			pp_leave(self, ceil);
			return r;
		}
		/* the ceiling changed while we waited: take on the new one */
		if ((int)__M_CEIL(m->_m_type) != ceil) {
			int e = pp_enter(self, (int)__M_CEIL(m->_m_type));
			pp_leave(self, ceil);
			if (e) {
				unlock(m, 0);
				return e;
			}
		}
		return r;
	}
	return acquire(m, clk, at, try);
}

/* the lock word of the futexes (not PI) */
static int acquire(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct __pthread *self = __pthread_self();
	int tid = self->tid, type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST;
	int priv = !(type & __M_SHARED), waited = 0, spins = 0, r;
	volatile int *l = LK(m);
	unsigned v;

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
		robust_add(self, m);
		self->robust_list.pending = 0;
		if (v & FUTEX_OWNER_DIED) {
			m->_m_type |= __M_INCONSISTENT;
			return EOWNERDEAD;
		}
	}
	return 0;
}


static long futex_pi(volatile int *l, int op, const struct timespec *at, int priv)
{
	return __syscall6(SYS_futex, (long)l, op | (priv ? FUTEX_PRIVATE : 0), 0, (long)at, 0, 0);
}

static void release_pi(pthread_mutex_t *m, int tid, int priv)
{
	if (a_cas(LK(m), tid, 0) != tid)
		futex_pi(LK(m), FUTEX_UNLOCK_PI, 0, priv);
}

/* PTHREAD_PRIO_INHERIT: free → ours by a compare-and-swap; otherwise the
 * kernel queues us (by priority) and boosts the owner */
static int acquire_pi(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct __pthread *self = __pthread_self();
	int tid = self->tid, type = m->_m_type, robust = type & __M_ROBUST, priv = !(type & __M_SHARED);
	volatile int *l = LK(m);
	unsigned v;
	long r = 0;

	if (robust) {
		__robust_register(self);
		self->robust_list.pending = TAGGED(m);
	}
	if (type & __M_PI_NOTREC) {
		r = -ENOTRECOVERABLE;
		goto out;
	}
	v = (unsigned)a_cas(l, 0, tid);
	if ((int)(v & TID_MASK) == tid && !try) {
		/* a normal mutex locked again by its owner (the kernel would
		 * answer EDEADLK): deadlock, or time out, as POSIX says */
		int dummy = 0;
		for (;;) {
			int e = __timedwait(&dummy, 0, clk, at, 1);
			if (e == ETIMEDOUT || e == EINVAL) {
				r = -e;
				goto out;
			}
		}
	}
	if (v) {
		if (try) {
			/* only a dead owner's mutex is left to the kernel */
			if ((v & TID_MASK) || !(v & FUTEX_OWNER_DIED)) {
				r = -EBUSY;
				goto out;
			}
			r = futex_pi(l, FUTEX_TRYLOCK_PI, 0, priv);
			if (r == -EAGAIN || r == -EWOULDBLOCK || r == -EDEADLK)
				r = -EBUSY;
		} else if (!at) {
			r = futex_pi(l, FUTEX_LOCK_PI, 0, priv);
		} else {
			if ((unsigned long)at->tv_nsec >= 1000000000UL) {
				r = -EINVAL;
				goto out;
			}
			if (at->tv_sec < 0) {
				r = -ETIMEDOUT;
				goto out;
			}
			r = futex_pi(l, FUTEX_LOCK_PI2 | (clk == CLOCK_REALTIME ? FUTEX_CLOCK_REALTIME : 0), at, priv);
			if (r == -ENOSYS && clk == CLOCK_REALTIME)
				r = futex_pi(l, FUTEX_LOCK_PI, at, priv);   /* before Linux 5.14 */
		}
		if (r)
			goto out;
	}
	/* ours; if the mutex was made unrecoverable meanwhile, pass it on */
	if (m->_m_type & __M_PI_NOTREC) {
		release_pi(m, tid, priv);
		r = -ENOTRECOVERABLE;
		goto out;
	}
	if ((type & 3) == PTHREAD_MUTEX_RECURSIVE)
		m->_m_count = 0;
	if (robust) {
		robust_add(self, m);
		self->robust_list.pending = 0;
		if ((v = (unsigned)*l) & FUTEX_OWNER_DIED) {
			/* ours now: only the kernel may still add FUTEX_WAITERS */
			while ((unsigned)a_cas(l, (int)v, (int)(v & ~FUTEX_OWNER_DIED)) != v)
				v = (unsigned)*l;
			m->_m_type |= __M_INCONSISTENT;
			return EOWNERDEAD;
		}
	}
	return 0;
out:
	if (robust)
		self->robust_list.pending = 0;
	return (int)-r;
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

/* pp: also give up the mutex's priority ceiling */
static int unlock(pthread_mutex_t *m, int pp)
{
	struct __pthread *self = __pthread_self();
	int type = m->_m_type, kind = type & 3, robust = type & __M_ROBUST, priv = !(type & __M_SHARED);
	volatile int *l = LK(m);
	unsigned v = (unsigned)*l;
	int nr = 0;

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
		nr = (type & __M_INCONSISTENT) != 0;
		self->robust_list.pending = TAGGED(m);
		*prev = next;
		if (next != (void *)&self->robust_list.head)
			MUTEX_OF(next)->_m_prev = prev;
		if (nr)
			m->_m_type &= ~__M_INCONSISTENT;
	}
	if (type & __M_PI) {
		/* unrecoverable: a flag, since the kernel may own the word */
		if (nr)
			m->_m_type |= __M_PI_NOTREC;
		release_pi(m, self->tid, priv);
		if (robust)
			self->robust_list.pending = 0;
		return 0;
	}
	v = (unsigned)a_swap(l, nr ? __M_NOTRECOVERABLE : 0);
	if (robust)
		self->robust_list.pending = 0;
	if (nr)
		__wake(l, -1, priv);
	else if (v & FUTEX_WAITERS)
		__wake(l, 1, priv);
	if ((type & __M_PP) && pp)
		pp_leave(self, (int)__M_CEIL(type));
	return 0;
}

int __pthread_mutex_unlock(pthread_mutex_t *m)
{
	return unlock(m, 1);
}

/* pthread_mutex_setprioceiling: take the mutex without its protocol, set
 * the ceiling, give it back */
int __pthread_mutex_setceiling(pthread_mutex_t *m, int prio, int *old)
{
	int r, type = m->_m_type;
	if (!(type & __M_PP) || prio < 1 || prio > 99)
		return EINVAL;
	r = acquire(m, CLOCK_REALTIME, 0, 0);
	if (r && r != EOWNERDEAD)
		return r;
	if (old)
		*old = (int)__M_CEIL(type);
	m->_m_type = (m->_m_type & ~(255 << 16)) | prio << 16;
	unlock(m, 0);
	return 0;
}

extern __typeof(__pthread_mutex_init) pthread_mutex_init __attribute__((weak, alias("__pthread_mutex_init")));
extern __typeof(__pthread_mutex_destroy) pthread_mutex_destroy __attribute__((weak, alias("__pthread_mutex_destroy")));
extern __typeof(__pthread_mutex_lock) pthread_mutex_lock __attribute__((weak, alias("__pthread_mutex_lock")));
extern __typeof(__pthread_mutex_trylock) pthread_mutex_trylock __attribute__((weak, alias("__pthread_mutex_trylock")));
extern __typeof(__pthread_mutex_timedlock) pthread_mutex_timedlock __attribute__((weak, alias("__pthread_mutex_timedlock")));
extern __typeof(__pthread_mutex_clocklock) pthread_mutex_clocklock __attribute__((weak, alias("__pthread_mutex_clocklock")));
extern __typeof(__pthread_mutex_unlock) pthread_mutex_unlock __attribute__((weak, alias("__pthread_mutex_unlock")));
