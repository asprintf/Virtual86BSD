/* <aio.h> with threads: every request gets a (detached) thread of its
 * own that performs the I/O with the ordinary system call.  Ordering:
 * a write waits for earlier writes on the same descriptor, aio_fsync for
 * all earlier requests on it; reads run at once.  Requests in progress
 * are on a list in submission order, under `lock'; each completion
 * increments `seq' and wakes its futex waiters (aio_suspend, lio_listio,
 * aio_cancel and requests waiting for their predecessors).
 *
 * The worker threads block every signal except SIGCANCEL: aio_cancel
 * cancels them (the I/O call and the wait for predecessors are
 * cancellation points), and the cleanup handler records ECANCELED.  A
 * SIGEV_THREAD notification function runs in the worker, with the signal
 * mask of the thread that submitted the request. */
#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "atomic.h"
#include "lock.h"

#define OP_FSYNC  3
#define OP_DSYNC  4

struct lio {
	volatile int pending;
	struct sigevent sev;
	unsigned long mask;
};

struct req {
	struct aiocb *cb;
	int op, fd;
	struct sigevent sev;
	unsigned long mask;          /* the submitter's signal mask */
	pthread_t td;
	struct req *next, *prev;
	struct lio *lio;
	int cancelling;
	volatile int *notcancelled;   /* aio_cancel: counts the requests not cancelled */
};

static volatile int lock;
static volatile int seq;
static struct req *head, *tail;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void unlink_req(struct req *r)
{
	if (r->prev) r->prev->next = r->next; else head = r->next;
	if (r->next) r->next->prev = r->prev; else tail = r->prev;
}

static void prepare(void) { LOCK(&lock); }
static void parent(void) { UNLOCK(&lock); }
static void child(void) { head = tail = 0; lock = 0; }
static void init(void) { pthread_atfork(prepare, parent, child); }

static void notify(const struct sigevent *sev, unsigned long mask)
{
	siginfo_t si;
	switch (sev->sigev_notify) {
	case SIGEV_SIGNAL:
		if (!sev->sigev_signo)
			break;
		memset(&si, 0, sizeof si);
		si.si_signo = sev->sigev_signo;
		si.si_code = SI_ASYNCIO;
		si.si_value = sev->sigev_value;
		si.si_pid = (pid_t)__syscall0(SYS_getpid);
		si.si_uid = (uid_t)__syscall0(SYS_getuid);
		__syscall3(SYS_rt_sigqueueinfo, si.si_pid, si.si_signo, &si);
		break;
	case SIGEV_THREAD:
		__restore_sigs(&mask);
		sev->sigev_notify_function(sev->sigev_value);
		__block_all_sigs(0);
		break;
	}
}

static void finish(struct req *r, ssize_t ret, int err)
{
	struct aiocb *cb = r->cb;
	struct lio *l = r->lio;
	LOCK(&lock);
	if (r->notcancelled && err != ECANCELED)
		a_inc(r->notcancelled);
	unlink_req(r);
	cb->__ret = ret;
	a_store(&cb->__err, err);
	UNLOCK(&lock);
	a_inc(&seq);
	__wake(&seq, -1, 1);
	notify(&r->sev, r->mask);
	if (l && a_fetch_add(&l->pending, -1) == 1) {
		notify(&l->sev, l->mask);
		free(l);
	}
	free(r);
}

static void cancelled(void *p)
{
	struct req *r = p;
	__setcancelstate(1, 0);
	finish(r, -1, ECANCELED);
}

/* must r wait for q (earlier, same descriptor)? */
static int conflicts(const struct req *r, const struct req *q)
{
	return r->op >= OP_FSYNC || (r->op == LIO_WRITE && q->op == LIO_WRITE);
}

static void *worker(void *p)
{
	struct req *r = p, *q;
	struct aiocb *cb = r->cb;
	unsigned long cancelmask = 1UL << (SIGCANCEL - 1);
	ssize_t ret = 0;
	int s, err;

	__syscall4(SYS_rt_sigprocmask, 1 /* SIG_UNBLOCK */, &cancelmask, 0, 8);
	pthread_cleanup_push(cancelled, r);
	for (;;) {
		LOCK(&lock);
		s = seq;
		for (q = r->prev; q && !(q->fd == r->fd && conflicts(r, q)); q = q->prev)
			;
		UNLOCK(&lock);
		if (!q)
			break;
		__timedwait_cp(&seq, s, CLOCK_MONOTONIC, 0, 1);
		__testcancel();
	}
	switch (r->op) {
	case LIO_READ:
		ret = pread(r->fd, (void *)cb->aio_buf, cb->aio_nbytes, cb->aio_offset);
		if (ret < 0 && errno == ESPIPE)      /* not seekable: the offset is ignored */
			ret = read(r->fd, (void *)cb->aio_buf, cb->aio_nbytes);
		break;
	case LIO_WRITE:
		/* with O_APPEND aio_write appends: the system call pwrite64 does
		   (pwrite() itself keeps to the offset, as POSIX has it) */
		ret = __syscall_ret((unsigned long)__syscall_cp4(SYS_pwrite64, r->fd, cb->aio_buf, cb->aio_nbytes, cb->aio_offset));
		if (ret < 0 && errno == ESPIPE)
			ret = write(r->fd, (const void *)cb->aio_buf, cb->aio_nbytes);
		break;
	case OP_FSYNC:
		ret = fsync(r->fd);
		break;
	case OP_DSYNC:
		ret = fdatasync(r->fd);
		break;
	}
	err = ret < 0 ? errno : 0;
	__setcancelstate(1, 0);
	pthread_cleanup_pop(0);
	finish(r, ret, err);
	return 0;
}

static int valid_sev(const struct sigevent *sev)
{
	switch (sev->sigev_notify) {
	case SIGEV_NONE:
		return 1;
	case SIGEV_SIGNAL:
		/* signal 0: none (a zeroed sigevent is SIGEV_SIGNAL on Linux) */
		return sev->sigev_signo >= 0 && sev->sigev_signo <= 64;
	case SIGEV_THREAD:
		return sev->sigev_notify_function != 0;
	}
	return 0;
}

static int submit(struct aiocb *cb, int op, struct lio *l)
{
	struct req *r;
	pthread_attr_t a;
	unsigned long old;
	int e;

	/* A bad aio_fildes is reported through the error status (EBADF from
	   the worker), so one bad request in a lio_listio() list does not
	   stop the others (aio_fsync() checks it itself).  aio_reqprio and
	   aio_offset do not apply to aio_fsync(). */
	if (!valid_sev(&cb->aio_sigevent) || (op <= LIO_WRITE
	    && (cb->aio_reqprio < 0 || cb->aio_reqprio > AIO_PRIO_DELTA_MAX || cb->aio_offset < 0))) {
		errno = EINVAL;
		return -1;
	}
	pthread_once(&once, init);
	if (!(r = malloc(sizeof *r))) {
		errno = EAGAIN;
		return -1;
	}
	r->cb = cb;
	r->op = op;
	r->fd = cb->aio_fildes;
	r->sev = cb->aio_sigevent;
	r->lio = l;
	r->cancelling = 0;
	r->notcancelled = 0;
	pthread_attr_init(&a);
	if (r->sev.sigev_notify == SIGEV_THREAD && r->sev.sigev_notify_attributes)
		a = *r->sev.sigev_notify_attributes;
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
	cb->__ret = 0;
	cb->__err = EINPROGRESS;
	cb->__req = r;
	__block_all_sigs(&old);
	r->mask = old;
	LOCK(&lock);
	r->next = 0;
	r->prev = tail;
	if (tail) tail->next = r; else head = r;
	tail = r;
	e = pthread_create(&r->td, &a, worker, r);
	if (e) {
		unlink_req(r);
		cb->__err = EAGAIN;
		cb->__ret = -1;
	}
	UNLOCK(&lock);
	__restore_sigs(&old);
	if (e) {
		free(r);
		errno = EAGAIN;
		return -1;
	}
	return 0;
}

int aio_read(struct aiocb *cb)
{
	return submit(cb, LIO_READ, 0);
}

int aio_write(struct aiocb *cb)
{
	return submit(cb, LIO_WRITE, 0);
}

int aio_fsync(int op, struct aiocb *cb)
{
	if (op != O_SYNC && op != O_DSYNC) {
		errno = EINVAL;
		return -1;
	}
	/* aio_fsync() itself shall fail with EBADF (aio_read and aio_write
	   may report it through the error status instead) */
	if (__syscall2(SYS_fcntl, cb->aio_fildes, F_GETFD) < 0) {
		errno = EBADF;
		return -1;
	}
	return submit(cb, op == O_SYNC ? OP_FSYNC : OP_DSYNC, 0);
}

int aio_error(const struct aiocb *cb)
{
	a_barrier();
	return cb->__err;
}

ssize_t aio_return(struct aiocb *cb)
{
	ssize_t r = cb->__ret;
	if (cb->__err == EINPROGRESS) {
		errno = EINVAL;
		return -1;
	}
	return r;
}

/* wait for seq to move on from s; 0, or EAGAIN (timeout), EINTR */
static int waitseq(int s, const struct timespec *at)
{
	int r = __timedwait_cp(&seq, s, CLOCK_MONOTONIC, at, 1);
	if (r == ETIMEDOUT)
		return EAGAIN;
	return r == EINTR ? EINTR : 0;
}

int aio_suspend(const struct aiocb *const list[], int n, const struct timespec *timeout)
{
	struct timespec at;
	int i, s, r;
	if (n < 0) {
		errno = EINVAL;
		return -1;
	}
	if (timeout) {
		clock_gettime(CLOCK_MONOTONIC, &at);
		at.tv_sec += timeout->tv_sec;
		if ((at.tv_nsec += timeout->tv_nsec) >= 1000000000) {
			at.tv_sec++;
			at.tv_nsec -= 1000000000;
		}
	}
	for (;;) {
		s = seq;
		a_barrier();
		for (i = 0; i < n; i++)
			if (list[i] && list[i]->__err != EINPROGRESS)
				return 0;
		if ((r = waitseq(s, timeout ? &at : 0))) {
			errno = r;
			return -1;
		}
	}
}

int aio_cancel(int fd, struct aiocb *cb)
{
	struct req *r;
	volatile int notcancelled = 0;
	int found = 0, s;
	if (__syscall2(SYS_fcntl, fd, F_GETFD) < 0) {
		errno = EBADF;
		return -1;
	}
	if (cb && cb->aio_fildes != fd) {
		errno = EINVAL;
		return -1;
	}
	LOCK(&lock);
	for (r = head; r; r = r->next)
		if (r->fd == fd && (!cb || r->cb == cb)) {
			if (!r->cancelling)
				pthread_cancel(r->td);
			r->cancelling = 1;
			r->notcancelled = &notcancelled;
			found = 1;
		}
	UNLOCK(&lock);
	if (!found)
		return AIO_ALLDONE;
	/* wait until they have finished, cancelled or not */
	for (;;) {
		LOCK(&lock);
		s = seq;
		for (r = head; r && r->notcancelled != &notcancelled; r = r->next)
			;
		UNLOCK(&lock);
		if (!r)
			break;
		__timedwait(&seq, s, CLOCK_MONOTONIC, 0, 1);
	}
	return notcancelled ? AIO_NOTCANCELED : AIO_CANCELED;
}

int lio_listio(int mode, struct aiocb *__RESTRICT const list[__RESTRICT], int n, struct sigevent *__RESTRICT sev)
{
	struct lio *l = 0;
	int i, fail = 0, again = 0, s;
	if ((mode != LIO_WAIT && mode != LIO_NOWAIT) || n < 0 || (mode == LIO_NOWAIT && sev && !valid_sev(sev))) {
		errno = EINVAL;
		return -1;
	}
	if (mode == LIO_NOWAIT && sev && sev->sigev_notify != SIGEV_NONE) {
		if (!(l = malloc(sizeof *l))) {
			errno = EAGAIN;
			return -1;
		}
		l->pending = 1;
		l->sev = *sev;
		__block_all_sigs(&l->mask);
		__restore_sigs(&l->mask);
	}
	for (i = 0; i < n; i++) {
		struct aiocb *cb = list[i];
		if (!cb || cb->aio_lio_opcode == LIO_NOP)
			continue;
		if (cb->aio_lio_opcode != LIO_READ && cb->aio_lio_opcode != LIO_WRITE) {
			cb->__err = EINVAL;
			cb->__ret = -1;
			fail = 1;
			continue;
		}
		if (l)
			a_inc(&l->pending);
		if (submit(cb, cb->aio_lio_opcode, l) < 0) {
			if (l)
				a_dec(&l->pending);
			if (errno == EAGAIN)
				again = 1;
			cb->__err = errno;
			cb->__ret = -1;
			fail = 1;
		}
	}
	if (l && a_fetch_add(&l->pending, -1) == 1) {
		/* everything finished already (or nothing was submitted) */
		notify(&l->sev, l->mask);
		free(l);
	}
	if (mode == LIO_WAIT) {
		for (;;) {
			s = seq;
			a_barrier();
			for (i = 0; i < n; i++)
				if (list[i] && list[i]->aio_lio_opcode != LIO_NOP && list[i]->__err == EINPROGRESS)
					break;
			if (i == n)
				break;
			if (waitseq(s, 0) == EINTR) {
				errno = EINTR;
				return -1;
			}
		}
		for (i = 0; i < n; i++)
			if (list[i] && list[i]->aio_lio_opcode != LIO_NOP && list[i]->__err)
				fail = 1;
	}
	if (again) {
		errno = EAGAIN;
		return -1;
	}
	if (fail) {
		errno = EIO;
		return -1;
	}
	return 0;
}
