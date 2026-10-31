/* POSIX timers on the kernel's.  A SIGEV_THREAD timer has a thread of
 * its own that waits for the timer's signal (SIGTIMER, sent to that
 * thread alone: SIGEV_THREAD_ID) and calls the notification function;
 * its timer_t is the thread's address, marked by the top bit (kernel
 * timer IDs are small non-negative numbers). */
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "atomic.h"

#define SIGEV_THREAD_ID 4

struct ksigevent {
	union sigval value;
	int signo, notify, tid;
	int pad[11];
};

struct start {
	void (*fn)(union sigval);
	union sigval val;
	volatile int ready;
};

static void *timer_thread(void *p)
{
	struct start *s = p;
	struct __pthread *self = __pthread_self();
	void (*fn)(union sigval) = s->fn;
	union sigval val = s->val;
	unsigned long set = 1UL << (SIGTIMER - 1);
	siginfo_t si;
	long r;
	int id;

	a_store(&s->ready, 1);
	__wake(&s->ready, 1, 1);
	for (;;) {
		r = __syscall4(SYS_rt_sigtimedwait, &set, &si, 0, 8);
		id = self->timer_id;
		if (id < 0)
			break;
		if (r == SIGTIMER && si.si_code == SI_TIMER)
			fn(val);
	}
	if (id != -1)
		__syscall1(SYS_timer_delete, id & INT_MAX);
	return 0;
}

static struct __pthread *thread_of(timer_t t)
{
	return (long)t < 0 ? (struct __pthread *)((unsigned long)t << 1) : 0;
}

static int kid(timer_t t)
{
	struct __pthread *td = thread_of(t);
	return td ? td->timer_id : (int)(long)t;
}

static int thread_timer(clockid_t c, struct sigevent *sev, timer_t *res)
{
	char attr[56];          /* pthread_attr_t */
	long *a = (long *)attr;
	struct start s;
	struct __pthread *td;
	struct ksigevent ksev;
	unsigned long set, block = 1UL << (SIGTIMER - 1);
	int id, r;

	if (sev->sigev_notify_attributes)
		memcpy(attr, sev->sigev_notify_attributes, sizeof attr);
	else {
		memset(attr, 0, sizeof attr);
		a[0] = 2L << 20;        /* stack size */
		a[1] = 4096;            /* guard */
	}
	a[3] = 1;                       /* detached */
	s.fn = sev->sigev_notify_function;
	s.val = sev->sigev_value;
	s.ready = 0;
	/* the thread inherits the mask with SIGTIMER blocked */
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &block, &set, 8);
	r = __pthread_create(&td, attr, timer_thread, &s);
	__restore_sigs(&set);
	if (r) {
		errno = r;
		return -1;
	}
	while (!s.ready)
		__wait(&s.ready, 0, 1);

	memset(&ksev, 0, sizeof ksev);
	ksev.value.sival_ptr = 0;
	ksev.signo = SIGTIMER;
	ksev.notify = SIGEV_THREAD_ID;
	ksev.tid = td->tid;
	r = (int)__syscall3(SYS_timer_create, c, &ksev, &id);
	if (r < 0) {
		td->timer_id = -1;
		__syscall2(SYS_tkill, td->tid, SIGTIMER);
		errno = -r;
		return -1;
	}
	td->timer_id = id;
	*res = (timer_t)((unsigned long)td >> 1 | 1UL << 63);
	return 0;
}

int timer_create(clockid_t c, struct sigevent *__RESTRICT sev, timer_t *__RESTRICT res)
{
	struct ksigevent ksev;
	int id;
	if (sev && sev->sigev_notify == SIGEV_THREAD)
		return thread_timer(c, sev, res);
	if (sev && sev->sigev_notify != SIGEV_SIGNAL && sev->sigev_notify != SIGEV_NONE) { errno = EINVAL; return -1; }
	memset(&ksev, 0, sizeof ksev);
	if (sev) {
		ksev.value = sev->sigev_value;
		ksev.signo = sev->sigev_signo;
		ksev.notify = sev->sigev_notify;
	} else {
		ksev.signo = SIGALRM;
		ksev.notify = SIGEV_SIGNAL;
	}
	if (__sys3(SYS_timer_create, c, sev ? &ksev : 0, &id) < 0) return -1;
	*res = (timer_t)(long)id;
	return 0;
}

int timer_delete(timer_t t)
{
	struct __pthread *td = thread_of(t);
	if (td) {
		/* the thread deletes the kernel timer and ends */
		td->timer_id |= INT_MIN;
		__syscall2(SYS_tkill, td->tid, SIGTIMER);
		return 0;
	}
	return (int)__sys1(SYS_timer_delete, (long)t);
}

int timer_getoverrun(timer_t t) { return (int)__sys1(SYS_timer_getoverrun, kid(t)); }
int timer_gettime(timer_t t, struct itimerspec *v) { return (int)__sys2(SYS_timer_gettime, kid(t), v); }
int timer_settime(timer_t t, int flags, const struct itimerspec *__RESTRICT v, struct itimerspec *__RESTRICT old)
{
	return (int)__sys4(SYS_timer_settime, kid(t), flags, v, old);
}
