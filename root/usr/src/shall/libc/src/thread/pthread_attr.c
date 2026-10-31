/* Thread attributes (the layout: pthread_impl.h). */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include "pthread_impl.h"
#include "libc.h"

int pthread_attr_init(pthread_attr_t *a)
{
	memset(a, 0, sizeof *a);
	a->_a_stacksize = 2L << 20;
	a->_a_guardsize = (long)__libc.page_size;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }

int pthread_attr_getdetachstate(const pthread_attr_t *a, int *s) { *s = (int)a->_a_detach; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->_a_detach = s;
	return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *__RESTRICT a, size_t *__RESTRICT s) { *s = (size_t)a->_a_guardsize; return 0; }
int pthread_attr_setguardsize(pthread_attr_t *a, size_t s)
{
	if (s > SIZE_MAX / 8) return EINVAL;
	a->_a_guardsize = (long)s;
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *__RESTRICT a, size_t *__RESTRICT s) { *s = (size_t)a->_a_stacksize; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t s)
{
	if (s < PTHREAD_STACK_MIN || s > SIZE_MAX / 4) return EINVAL;
	a->_a_stackaddr = 0;
	a->_a_stacksize = (long)s;
	return 0;
}

int pthread_attr_getstack(const pthread_attr_t *__RESTRICT a, void **__RESTRICT addr, size_t *__RESTRICT s)
{
	if (!a->_a_stackaddr) return EINVAL;
	*addr = (void *)a->_a_stackaddr;
	*s = (size_t)a->_a_stacksize;
	return 0;
}

int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t s)
{
	if (s < PTHREAD_STACK_MIN || s > SIZE_MAX / 4) return EINVAL;
	a->_a_stackaddr = (long)addr;
	a->_a_stacksize = (long)s;
	return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *__RESTRICT a, int *__RESTRICT s) { *s = (int)(a->_a_sched & 1); return 0; }
int pthread_attr_setinheritsched(pthread_attr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->_a_sched = (a->_a_sched & ~1L) | s;
	return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *__RESTRICT a, int *__RESTRICT p) { *p = (int)(a->_a_sched >> 8); return 0; }
int pthread_attr_setschedpolicy(pthread_attr_t *a, int p)
{
	if (p != SCHED_OTHER && p != SCHED_FIFO && p != SCHED_RR) return EINVAL;
	a->_a_sched = (a->_a_sched & 0xff) | (long)p << 8;
	return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *__RESTRICT a, struct sched_param *__RESTRICT p)
{
	p->sched_priority = (int)a->_a_prio;
	return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *__RESTRICT a, const struct sched_param *__RESTRICT p)
{
	a->_a_prio = p->sched_priority;
	return 0;
}

int pthread_attr_getscope(const pthread_attr_t *__RESTRICT a, int *__RESTRICT s) { (void)a; *s = PTHREAD_SCOPE_SYSTEM; return 0; }
int pthread_attr_setscope(pthread_attr_t *a, int s)
{
	(void)a;
	if (s == PTHREAD_SCOPE_SYSTEM) return 0;
	return s == PTHREAD_SCOPE_PROCESS ? ENOTSUP : EINVAL;
}
