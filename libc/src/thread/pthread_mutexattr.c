/* Mutex attributes, and the rest of the mutex functions (apart from
 * pthread_mutex.c, which the C11 mtx_ functions use). */
#include <errno.h>
#include <pthread.h>
#include "pthread_impl.h"

#define TID_MASK 0x3fffffff

int pthread_mutex_consistent(pthread_mutex_t *m)
{
	if ((m->_m_type & (__M_ROBUST | __M_INCONSISTENT)) != (__M_ROBUST | __M_INCONSISTENT) ||
	    (int)((unsigned)m->__vi[1] & TID_MASK) != __pthread_self()->tid)
		return EINVAL;
	m->_m_type &= ~__M_INCONSISTENT;
	return 0;
}

/* priority protection is not supported */
int pthread_mutex_getprioceiling(const pthread_mutex_t *__RESTRICT m, int *__RESTRICT p)
{
	(void)m; (void)p;
	return EINVAL;
}

int pthread_mutex_setprioceiling(pthread_mutex_t *__RESTRICT m, int p, int *__RESTRICT old)
{
	(void)m; (void)p; (void)old;
	return EINVAL;
}

/* attributes: type in bits 0-1, robust 4, process-shared 128 */
int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->__attr = 0; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }

int pthread_mutexattr_gettype(const pthread_mutexattr_t *__RESTRICT a, int *__RESTRICT t) { *t = (int)(a->__attr & 3); return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t)
{
	if ((unsigned)t > 2) return EINVAL;
	a->__attr = (a->__attr & ~3u) | (unsigned)t;
	return 0;
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__RESTRICT a, int *__RESTRICT r) { *r = (a->__attr & __M_ROBUST) != 0; return 0; }
int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int r)
{
	if ((unsigned)r > 1) return EINVAL;
	a->__attr = (a->__attr & ~(unsigned)__M_ROBUST) | (r ? __M_ROBUST : 0);
	return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__RESTRICT a, int *__RESTRICT s) { *s = (a->__attr & __M_SHARED) != 0; return 0; }
int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->__attr = (a->__attr & ~(unsigned)__M_SHARED) | (s ? __M_SHARED : 0);
	return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__RESTRICT a, int *__RESTRICT p) { (void)a; *p = PTHREAD_PRIO_NONE; return 0; }
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int p)
{
	(void)a;
	if (p == PTHREAD_PRIO_NONE) return 0;
	return p == PTHREAD_PRIO_INHERIT || p == PTHREAD_PRIO_PROTECT ? ENOTSUP : EINVAL;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__RESTRICT a, int *__RESTRICT p) { (void)a; (void)p; return EINVAL; }
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a, int p) { (void)a; (void)p; return EINVAL; }
