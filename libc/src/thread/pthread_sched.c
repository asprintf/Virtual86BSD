/* Scheduling of threads, and their CPU-time clocks. */
#include <errno.h>
#include <pthread.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "lock.h"

int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *p)
{
	unsigned long set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	r = t->tid ? (int)-__syscall3(SYS_sched_setscheduler, t->tid, policy, p) : ESRCH;
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

int pthread_getschedparam(pthread_t t, int *__RESTRICT policy, struct sched_param *__RESTRICT p)
{
	unsigned long set;
	long r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	if (!t->tid)
		r = -ESRCH;
	else if ((r = __syscall2(SYS_sched_getparam, t->tid, p)) >= 0) {
		r = __syscall1(SYS_sched_getscheduler, t->tid);
		if (r >= 0) {
			*policy = (int)r;
			r = 0;
		}
	}
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return (int)-r;
}

int pthread_setschedprio(pthread_t t, int prio)
{
	unsigned long set;
	int r;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	r = t->tid ? (int)-__syscall2(SYS_sched_setparam, t->tid, &prio) : ESRCH;
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

/* the kernel's per-thread scheduler clock: ~tid << 3 | PERTHREAD | SCHED */
int pthread_getcpuclockid(pthread_t t, clockid_t *c)
{
	*c = (clockid_t)(~(unsigned)t->tid << 3 | 6);
	return 0;
}
