/* Unnamed semaphores: __val[0] the value, [1] the number of sleepers,
 * [2] process-shared.  A sleeper waits on the value word being 0, so a
 * post between its test and its sleep makes the sleep return at once. */
#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include "pthread_impl.h"
#include "atomic.h"

int sem_init(sem_t *s, int shared, unsigned value)
{
	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}
	s->__val[0] = (int)value;
	s->__val[1] = 0;
	s->__val[2] = shared ? 1 : 0;
	s->__val[3] = 0;
	return 0;
}

int sem_destroy(sem_t *s) { (void)s; return 0; }

int sem_getvalue(sem_t *__RESTRICT s, int *__RESTRICT v)
{
	*v = s->__val[0];
	return 0;
}

int sem_post(sem_t *s)
{
	int v;
	do {
		v = s->__val[0];
		if (v == SEM_VALUE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
	} while (a_cas(&s->__val[0], v, v + 1) != v);
	if (s->__val[1])
		__wake(&s->__val[0], 1, !s->__val[2]);
	return 0;
}

static int dec(sem_t *s)
{
	int v;
	while ((v = s->__val[0]) > 0)
		if (a_cas(&s->__val[0], v, v - 1) == v)
			return 1;
	return 0;
}

int sem_trywait(sem_t *s)
{
	if (dec(s))
		return 0;
	errno = EAGAIN;
	return -1;
}

static int wait(sem_t *s, clockid_t clk, const struct timespec *at)
{
	int r;
	__testcancel();
	for (;;) {
		if (dec(s))
			return 0;
		a_inc(&s->__val[1]);
		r = __timedwait_cp(&s->__val[0], 0, clk, at, !s->__val[2]);
		a_dec(&s->__val[1]);
		if (r == ETIMEDOUT || r == EINTR || r == EINVAL) {
			if (dec(s))
				return 0;
			errno = r;
			return -1;
		}
	}
}

int sem_wait(sem_t *s) { return wait(s, CLOCK_REALTIME, 0); }

int sem_timedwait(sem_t *__RESTRICT s, const struct timespec *__RESTRICT at)
{
	return wait(s, CLOCK_REALTIME, at);
}

int sem_clockwait(sem_t *__RESTRICT s, clockid_t clk, const struct timespec *__RESTRICT at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC) {
		errno = EINVAL;
		return -1;
	}
	return wait(s, clk, at);
}
