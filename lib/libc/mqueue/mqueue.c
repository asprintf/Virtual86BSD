/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* <mqueue.h> on the kernel's message queues.  mq_notify with
 * SIGEV_THREAD: the kernel writes a notification record to a netlink
 * socket, which a thread of ours reads before calling the function. */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "atomic.h"
#include "lock.h"

#define SIGEV_THREAD_ID 4

mqd_t mq_open(const char *name, int flags, ...)
{
	mode_t mode = 0;
	struct mq_attr *attr = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int);
		attr = va_arg(ap, struct mq_attr *);
		va_end(ap);
	}
	return (mqd_t)__sys4(SYS_kmq_open, name, flags, mode, attr);
}

int mq_close(mqd_t q) { return close(q); }

int mq_unlink(const char *name)
{
	long r = __syscall1(SYS_kmq_unlink, name);
	if (r == -EPERM)
		r = -EACCES;          /* POSIX: EACCES */
	return (int)__syscall_ret((unsigned long)r);
}

int mq_getattr(mqd_t q, struct mq_attr *a) { return (int)__sys3(SYS_kmq_setattr, q, 0, a); }

int mq_setattr(mqd_t q, const struct mq_attr *__RESTRICT a, struct mq_attr *__RESTRICT old)
{
	return (int)__sys3(SYS_kmq_setattr, q, a, old);
}

int mq_timedsend(mqd_t q, const char *m, size_t n, unsigned prio, const struct timespec *at)
{
	return (int)__sys_cp5(SYS_kmq_timedsend, q, m, n, prio, at);
}

int mq_send(mqd_t q, const char *m, size_t n, unsigned prio) { return mq_timedsend(q, m, n, prio, 0); }

ssize_t mq_timedreceive(mqd_t q, char *__RESTRICT m, size_t n, unsigned *__RESTRICT prio,
                        const struct timespec *__RESTRICT at)
{
	return __sys_cp5(SYS_kmq_timedreceive, q, m, n, prio, at);
}

ssize_t mq_receive(mqd_t q, char *m, size_t n, unsigned *prio) { return mq_timedreceive(q, m, n, prio, 0); }

/* SIGEV_THREAD: a thread of its own waits for SIGTIMER, sent to it alone
 * (SIGEV_THREAD_ID) when a message arrives, and calls the function; the
 * registration is one-shot, as the kernel's.  Registered threads are kept
 * in a table, to end the one of a registration that is cancelled. */
struct start {
	void (*fn)(union sigval);
	union sigval val;
	volatile int ready, cancel, q, tid;
};

#define NTAB 32
static struct start *tab[NTAB];
static volatile int tablock;

static void forget(struct start *s)
{
	int i;
	LOCK(&tablock);
	for (i = 0; i < NTAB; i++)
		if (tab[i] == s)
			tab[i] = 0;
	UNLOCK(&tablock);
}

static void *notify_thread(void *p)
{
	struct start *s = p;
	void (*fn)(union sigval) = s->fn;
	union sigval val = s->val;
	__sm_t set = { { 1UL << (SIGTIMER - 1), 0 } };
	siginfo_t si;
	long r;

	s->tid = __pthread_self()->tid;
	a_store(&s->ready, 1);
	__wake(&s->ready, 1, 1);
	for (;;) {
		r = __syscall3(SYS_sigtimedwait, &set, &si, 0);
		if (s->cancel)
			break;
		if (r == SIGTIMER && si.si_code == SI_MESGQ) {
			forget(s);
			fn(val);
			break;
		}
	}
	forget(s);
	free(s);
	return 0;
}

static void cancel_thread(mqd_t q)
{
	int i;
	LOCK(&tablock);
	for (i = 0; i < NTAB; i++)
		if (tab[i] && tab[i]->q == q) {
			tab[i]->cancel = 1;
			__syscall2(SYS_thr_kill, tab[i]->tid, SIGTIMER);
			tab[i] = 0;
		}
	UNLOCK(&tablock);
}

int mq_notify(mqd_t q, const struct sigevent *sev)
{
	struct sigevent k;
	char attr[56];
	long *a = (long *)attr;
	struct start *s;
	struct __pthread *td;
	__sm_t set, block = { { 1UL << (SIGTIMER - 1), 0 } };
	int r, i;

	if (!sev || sev->sigev_notify != SIGEV_THREAD) {
		r = (int)__sys2(SYS_kmq_notify, q, sev);
		if (!r) cancel_thread(q);
		return r;
	}
	if (!(s = calloc(1, sizeof *s))) {
		errno = ENOMEM;
		return -1;
	}
	s->fn = sev->sigev_notify_function;
	s->val = sev->sigev_value;
	s->q = q;
	if (sev->sigev_notify_attributes)
		memcpy(attr, sev->sigev_notify_attributes, sizeof attr);
	else {
		memset(attr, 0, sizeof attr);
		a[0] = 2L << 20;
		a[1] = 4096;
	}
	a[3] = 1;                       /* detached */
	/* the thread inherits the mask with SIGTIMER blocked */
	__syscall3(SYS_sigprocmask, SIG_BLOCK, &block, &set);
	r = __pthread_create(&td, attr, notify_thread, s);
	__restore_sigs(&set);
	if (r) {
		free(s);
		errno = r;
		return -1;
	}
	while (!s->ready)
		__wait(&s->ready, 0, 1);
	memset(&k, 0, sizeof k);
	k.sigev_notify = SIGEV_THREAD_ID;
	k.sigev_signo = SIGTIMER;
	k.sigev_value = sev->sigev_value;
	k.__un.__tid = s->tid;
	if ((r = (int)__syscall2(SYS_kmq_notify, q, &k)) < 0) {
		s->cancel = 1;
		__syscall2(SYS_thr_kill, s->tid, SIGTIMER);
		errno = -r;
		return -1;
	}
	cancel_thread(q);               /* the registration it replaced */
	LOCK(&tablock);
	for (i = 0; i < NTAB && tab[i]; i++);
	if (i < NTAB)
		tab[i] = s;
	UNLOCK(&tablock);
	return 0;
}
