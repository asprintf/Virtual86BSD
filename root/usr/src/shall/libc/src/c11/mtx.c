/* <threads.h>: mutexes and condition variables, as pthread's. */
#include <errno.h>
#include <string.h>
#include <threads.h>

int __pthread_mutex_lock(mtx_t *);
int __pthread_mutex_trylock(mtx_t *);
int __pthread_mutex_timedlock(mtx_t *, const struct timespec *);
int __pthread_mutex_unlock(mtx_t *);
int __pthread_cond_signal(cnd_t *);
int __pthread_cond_broadcast(cnd_t *);
int __pthread_cond_wait(cnd_t *, mtx_t *);
int __pthread_cond_timedwait(cnd_t *, mtx_t *, const struct timespec *);

int mtx_init(mtx_t *m, int type)
{
	memset(m, 0, sizeof *m);
	if (type & mtx_recursive)
		m->__i[0] = 1;          /* PTHREAD_MUTEX_RECURSIVE */
	return thrd_success;
}

void mtx_destroy(mtx_t *m) { (void)m; }
int mtx_lock(mtx_t *m) { return __pthread_mutex_lock(m) ? thrd_error : thrd_success; }
int mtx_unlock(mtx_t *m) { return __pthread_mutex_unlock(m) ? thrd_error : thrd_success; }

int mtx_trylock(mtx_t *m)
{
	int r = __pthread_mutex_trylock(m);
	return !r ? thrd_success : r == EBUSY ? thrd_busy : thrd_error;
}

int mtx_timedlock(mtx_t *__RESTRICT m, const struct timespec *__RESTRICT at)
{
	int r = __pthread_mutex_timedlock(m, at);
	return !r ? thrd_success : r == ETIMEDOUT ? thrd_timedout : thrd_error;
}

int cnd_init(cnd_t *c) { memset(c, 0, sizeof *c); return thrd_success; }
void cnd_destroy(cnd_t *c) { (void)c; }
int cnd_signal(cnd_t *c) { return __pthread_cond_signal(c) ? thrd_error : thrd_success; }
int cnd_broadcast(cnd_t *c) { return __pthread_cond_broadcast(c) ? thrd_error : thrd_success; }
int cnd_wait(cnd_t *c, mtx_t *m) { return __pthread_cond_wait(c, m) ? thrd_error : thrd_success; }

int cnd_timedwait(cnd_t *__RESTRICT c, mtx_t *__RESTRICT m, const struct timespec *__RESTRICT at)
{
	int r = __pthread_cond_timedwait(c, m, at);
	return !r ? thrd_success : r == ETIMEDOUT ? thrd_timedout : thrd_error;
}
