/* Barriers and spin locks. */
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "atomic.h"

static void blk(volatile int *l, int priv)
{
	if (!a_cas(l, 0, 1))
		return;
	while (a_swap(l, 2))
		__wait(l, 2, priv);
}

static void bunlk(volatile int *l, int priv)
{
	if (a_swap(l, 0) == 2)
		__wake(l, 1, priv);
}

int pthread_barrier_init(pthread_barrier_t *__RESTRICT b, const pthread_barrierattr_t *__RESTRICT a, unsigned n)
{
	if (!n || n > 0x7fffffff)
		return EINVAL;
	memset(b, 0, sizeof *b);
	b->_b_limit = (int)n;
	b->_b_shared = a && a->__attr ? 1 : 0;
	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *b) { (void)b; return 0; }

int pthread_barrier_wait(pthread_barrier_t *b)
{
	int priv = !b->_b_shared, seq;
	blk(&b->__vi[0], priv);
	if (++b->_b_count == b->_b_limit) {
		b->_b_count = 0;
		a_inc(&b->__vi[3]);
		bunlk(&b->__vi[0], priv);
		__wake(&b->__vi[3], -1, priv);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}
	seq = b->_b_seq;
	bunlk(&b->__vi[0], priv);
	while (b->_b_seq == seq)
		__wait(&b->__vi[3], seq, priv);
	return 0;
}

int pthread_barrierattr_init(pthread_barrierattr_t *a) { a->__attr = 0; return 0; }
int pthread_barrierattr_destroy(pthread_barrierattr_t *a) { (void)a; return 0; }
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__RESTRICT a, int *__RESTRICT s) { *s = (int)a->__attr; return 0; }
int pthread_barrierattr_setpshared(pthread_barrierattr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->__attr = (unsigned)s;
	return 0;
}

int pthread_spin_init(pthread_spinlock_t *s, int shared) { (void)shared; *s = 0; return 0; }
int pthread_spin_destroy(pthread_spinlock_t *s) { (void)s; return 0; }
int pthread_spin_lock(pthread_spinlock_t *s)
{
	while (*s || a_cas(s, 0, 1))
		a_spin();
	return 0;
}
int pthread_spin_trylock(pthread_spinlock_t *s) { return a_cas(s, 0, 1) ? EBUSY : 0; }
int pthread_spin_unlock(pthread_spinlock_t *s) { a_store(s, 0); return 0; }
