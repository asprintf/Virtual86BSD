/* <threads.h>: threads and thread-specific storage, on the pthread
 * implementation (whose __ names keep the ISO namespace clean). */
#include <errno.h>
#include <string.h>
#include <threads.h>
#include "pthread_impl.h"
#include "libc.h"
#include "syscall.h"

void *__pthread_getspecific(unsigned);
int __pthread_setspecific(unsigned, const void *);
int __pthread_key_create(unsigned *, void (*)(void *));
int __pthread_key_delete(unsigned);

int thrd_create(thrd_t *t, thrd_start_t fn, void *arg)
{
	long attr[7];
	int r;
	memset(attr, 0, sizeof attr);
	attr[0] = 2L << 20;                    /* stack size */
	attr[1] = (long)__libc.page_size;      /* guard */
	attr[6] = 2;                           /* the start routine returns int */
	r = __pthread_create(t, attr, (void *(*)(void *))(void *)fn, arg);
	return !r ? thrd_success : r == EAGAIN ? thrd_nomem : thrd_error;
}

_Noreturn void thrd_exit(int r)
{
	__pthread_exit((void *)(long)r);
}

int thrd_join(thrd_t t, int *res)
{
	void *p;
	if (__pthread_join(t, &p))
		return thrd_error;
	if (res)
		*res = (int)(long)p;
	return thrd_success;
}

int thrd_detach(thrd_t t) { return __pthread_detach(t) ? thrd_error : thrd_success; }
thrd_t thrd_current(void) { return __pthread_self(); }
int thrd_equal(thrd_t a, thrd_t b) { return a == b; }
void thrd_yield(void) { __syscall0(SYS_sched_yield); }

/* 0, -1 when interrupted by a signal, -2 on another error */
int thrd_sleep(const struct timespec *d, struct timespec *rem)
{
	long r = __syscall_cp2(SYS_nanosleep, d, rem);
	return !r ? 0 : r == -EINTR ? -1 : -2;
}

int tss_create(tss_t *k, tss_dtor_t dtor) { return __pthread_key_create(k, dtor) ? thrd_error : thrd_success; }
void tss_delete(tss_t k) { __pthread_key_delete(k); }
void *tss_get(tss_t k) { return __pthread_getspecific(k); }
int tss_set(tss_t k, void *v) { return __pthread_setspecific(k, v) ? thrd_error : thrd_success; }

int __pthread_once(int *, void (*)(void));
void call_once(once_flag *f, void (*fn)(void)) { __pthread_once(f, fn); }
