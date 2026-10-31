/* Read-write locks: the lock word counts readers, or is 0x7fffffff when
 * write-locked; bit 31 marks sleepers, which every last unlock wakes.
 * Readers are preferred (as glibc's default). */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "atomic.h"

#define WR 0x7fffffff
#define WAITING ((int)0x80000000)
#define L(rw) (&(rw)->__vi[0])
#define PRIV(rw) (!(rw)->_rw_shared)
#define OWNER(rw) (&(rw)->__vi[3])  /* tid of the write owner, for EDEADLK */

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
	int v, n, r;
	if (!try && *OWNER(rw) == __pthread_self()->tid)
		return EDEADLK;
	for (;;) {
		v = *L(rw);
		n = v & WR;
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

static int wrlock(pthread_rwlock_t *rw, clockid_t clk, const struct timespec *at, int try)
{
	int v, r;
	if (!try && *OWNER(rw) == __pthread_self()->tid)
		return EDEADLK;
	for (;;) {
		v = *L(rw);
		if (!(v & WR)) {
			if (a_cas(L(rw), v, v | WR) == v) {
				*OWNER(rw) = __pthread_self()->tid;
				return 0;
			}
			continue;
		}
		if (try)
			return EBUSY;
		if ((r = sleep_on(rw, v, clk, at)))
			return r;
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
