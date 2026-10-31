/* Condition variable attributes: the clock in bits 0-7, process-shared
 * in bit 31. */
#include <errno.h>
#include <pthread.h>

int pthread_condattr_init(pthread_condattr_t *a) { a->__attr = 0; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }

int pthread_condattr_getclock(const pthread_condattr_t *__RESTRICT a, clockid_t *__RESTRICT c)
{
	*c = (clockid_t)(a->__attr & 0xff);
	return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t c)
{
	if (c != CLOCK_REALTIME && c != CLOCK_MONOTONIC)
		return EINVAL;
	a->__attr = (a->__attr & ~0xffu) | (unsigned)c;
	return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *__RESTRICT a, int *__RESTRICT s)
{
	*s = (int)(a->__attr >> 31);
	return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *a, int s)
{
	if ((unsigned)s > 1)
		return EINVAL;
	a->__attr = (a->__attr & 0x7fffffffu) | (unsigned)s << 31;
	return 0;
}
