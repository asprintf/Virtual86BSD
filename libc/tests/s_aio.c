/* Self-checking: <aio.h> (reads, writes, fsync, notification by signal
 * and by thread, aio_suspend with timeout and interruption, aio_cancel
 * on blocked pipe reads, lio_listio, ordering of appends, fork, prioritized
 * I/O) and <devctl.h>. */
#define _XOPEN_SOURCE 800
#include <aio.h>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "out.h"

static char path[] = "/tmp/s_aio.XXXXXX";

static void waitfor(struct aiocb *cb)
{
	const struct aiocb *l[1];
	l[0] = cb;
	while (aio_error(cb) == EINPROGRESS)
		aio_suspend(l, 1, 0);
}

static sem_t sem;
static volatile int tval;
static void thr_notify(union sigval v)
{
	tval = v.sival_int;
	sem_post(&sem);
}

static volatile int alarms;
static void on_alarm(int s) { (void)s; alarms++; }

static void basic(int fd)
{
	static char a[5000], b[3000], r[8000];
	struct aiocb wa, wb, rd, fs;
	const struct aiocb *list[3];
	int i;
	for (i = 0; i < 5000; i++) a[i] = (char)('a' + i % 26);
	for (i = 0; i < 3000; i++) b[i] = (char)('A' + i % 26);
	memset(&wa, 0, sizeof wa);
	wa.aio_fildes = fd; wa.aio_buf = a; wa.aio_nbytes = sizeof a; wa.aio_offset = 0;
	wb = wa;
	wb.aio_buf = b; wb.aio_nbytes = sizeof b; wb.aio_offset = 5000;
	CHECK(aio_write(&wa) == 0);
	CHECK(aio_write(&wb) == 0);
	list[0] = &wa; list[1] = 0; list[2] = &wb;
	while (aio_error(&wa) == EINPROGRESS || aio_error(&wb) == EINPROGRESS)
		CHECK(aio_suspend(list, 3, 0) == 0);
	CHECK(aio_error(&wa) == 0 && aio_return(&wa) == 5000);
	CHECK(aio_error(&wb) == 0 && aio_return(&wb) == 3000);
	memset(&fs, 0, sizeof fs);
	fs.aio_fildes = fd;
	CHECK(aio_fsync(O_SYNC, &fs) == 0);
	waitfor(&fs);
	CHECK(aio_error(&fs) == 0 && aio_return(&fs) == 0);
	CHECK(aio_fsync(O_DSYNC, &fs) == 0);
	waitfor(&fs);
	CHECK(aio_error(&fs) == 0);
	errno = 0;
	CHECK(aio_fsync(12345, &fs) == -1 && errno == EINVAL);
	fs.aio_fildes = -1;
	errno = 0;
	CHECK(aio_fsync(O_SYNC, &fs) == -1 && errno == EBADF);
	fs.aio_fildes = fd;
	/* aio_reqprio and aio_offset do not apply to aio_fsync() */
	fs.aio_reqprio = -1;
	fs.aio_offset = -1;
	CHECK(aio_fsync(O_SYNC, &fs) == 0);
	waitfor(&fs);
	CHECK(aio_error(&fs) == 0);
	memset(&rd, 0, sizeof rd);
	rd.aio_fildes = fd; rd.aio_buf = r; rd.aio_nbytes = sizeof r; rd.aio_offset = 0;
	CHECK(aio_read(&rd) == 0);
	waitfor(&rd);
	CHECK(aio_return(&rd) == 8000);
	CHECK(!memcmp(r, a, 5000) && !memcmp(r + 5000, b, 3000));
	/* past the end */
	rd.aio_offset = 7990;
	CHECK(aio_read(&rd) == 0);
	waitfor(&rd);
	CHECK(aio_return(&rd) == 10 && !memcmp(r, b + 2990, 10));
	/* errors */
	rd.aio_fildes = -1;
	errno = 0;
	CHECK(aio_read(&rd) == 0);	/* EBADF arrives through aio_error() */
	waitfor(&rd);
	CHECK(aio_error(&rd) == EBADF && aio_return(&rd) == -1);
	rd.aio_fildes = fd;
	rd.aio_offset = -1;
	errno = 0;
	CHECK(aio_read(&rd) == -1 && errno == EINVAL);
	rd.aio_offset = 0;
	rd.aio_reqprio = -1;
	errno = 0;
	CHECK(aio_read(&rd) == -1 && errno == EINVAL);
	rd.aio_reqprio = AIO_PRIO_DELTA_MAX + 1;
	errno = 0;
	CHECK(aio_read(&rd) == -1 && errno == EINVAL);
	rd.aio_reqprio = 0;
	/* a failing operation: reading a write-only descriptor */
	{
		int wfd = open(path, O_WRONLY);
		rd.aio_fildes = wfd;
		CHECK(aio_read(&rd) == 0);
		waitfor(&rd);
		CHECK(aio_error(&rd) == EBADF && aio_return(&rd) == -1);
		close(wfd);
		rd.aio_fildes = fd;
	}
}

static void notification(int fd)
{
	static char buf[100];
	struct aiocb cb;
	sigset_t set;
	siginfo_t si;
	memset(&cb, 0, sizeof cb);
	cb.aio_fildes = fd; cb.aio_buf = buf; cb.aio_nbytes = sizeof buf;
	/* by signal */
	sigemptyset(&set);
	sigaddset(&set, SIGRTMIN + 1);
	pthread_sigmask(SIG_BLOCK, &set, 0);
	cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	cb.aio_sigevent.sigev_signo = SIGRTMIN + 1;
	cb.aio_sigevent.sigev_value.sival_ptr = &cb;
	CHECK(aio_read(&cb) == 0);
	memset(&si, 0, sizeof si);
	CHECK(sigwaitinfo(&set, &si) == SIGRTMIN + 1);
	CHECK(si.si_code == SI_ASYNCIO);
	CHECK(si.si_value.sival_ptr == &cb);
	CHECK(aio_error(&cb) == 0 && aio_return(&cb) == 100);
	/* by thread */
	sem_init(&sem, 0, 0);
	cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
	cb.aio_sigevent.sigev_notify_function = thr_notify;
	cb.aio_sigevent.sigev_value.sival_int = 4242;
	cb.aio_sigevent.sigev_notify_attributes = 0;
	CHECK(aio_read(&cb) == 0);
	CHECK(sem_wait(&sem) == 0);
	CHECK(tval == 4242);
	CHECK(aio_error(&cb) == 0 && aio_return(&cb) == 100);
	/* an invalid sigevent */
	cb.aio_sigevent.sigev_notify = 77;
	errno = 0;
	CHECK(aio_read(&cb) == -1 && errno == EINVAL);
}

static void pipes(void)
{
	int p[2], q[2];
	static char buf[64];
	struct aiocb r1, r2, r3;
	const struct aiocb *list[2];
	struct timespec ts = { 0, 50000000 };
	struct sigaction sa;
	struct itimerspec it;
	timer_t tm;

	CHECK(pipe(p) == 0 && pipe(q) == 0);
	memset(&r1, 0, sizeof r1);
	r1.aio_fildes = p[0]; r1.aio_buf = buf; r1.aio_nbytes = sizeof buf;
	r2 = r1; r3 = r1;
	CHECK(aio_read(&r1) == 0);
	list[0] = &r1; list[1] = 0;
	errno = 0;
	CHECK(aio_suspend(list, 2, &ts) == -1 && errno == EAGAIN);
	CHECK(aio_error(&r1) == EINPROGRESS);
	/* interrupted by a signal */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_alarm;
	sigaction(SIGALRM, &sa, 0);
	CHECK(timer_create(CLOCK_MONOTONIC, 0, &tm) == 0);   /* SIGALRM */
	memset(&it, 0, sizeof it);
	it.it_value.tv_nsec = 30000000;
	timer_settime(tm, 0, &it, 0);
	errno = 0;
	CHECK(aio_suspend(list, 1, 0) == -1 && errno == EINTR);
	CHECK(alarms == 1);
	timer_delete(tm);
	/* completion by a write */
	CHECK(write(p[1], "hello", 5) == 5);
	waitfor(&r1);
	CHECK(aio_return(&r1) == 5 && !memcmp(buf, "hello", 5));
	/* cancellation of one request */
	CHECK(aio_read(&r1) == 0);
	nanosleep(&(struct timespec){ 0, 20000000 }, 0);
	CHECK(aio_cancel(p[0], &r1) == AIO_CANCELED);
	CHECK(aio_error(&r1) == ECANCELED && aio_return(&r1) == -1);
	CHECK(aio_cancel(p[0], &r1) == AIO_ALLDONE);
	CHECK(aio_cancel(p[0], 0) == AIO_ALLDONE);
	/* of all on a descriptor; another descriptor's stays */
	r3.aio_fildes = q[0];
	CHECK(aio_read(&r1) == 0 && aio_read(&r2) == 0 && aio_read(&r3) == 0);
	CHECK(aio_cancel(p[0], 0) == AIO_CANCELED);
	CHECK(aio_error(&r1) == ECANCELED && aio_error(&r2) == ECANCELED);
	CHECK(aio_error(&r3) == EINPROGRESS);
	errno = 0;
	CHECK(aio_cancel(q[0], &r1) == -1 && errno == EINVAL);
	CHECK(write(q[1], "x", 1) == 1);
	waitfor(&r3);
	CHECK(aio_return(&r3) == 1);
	errno = 0;
	CHECK(aio_cancel(-1, 0) == -1 && errno == EBADF);
	/* the pipe still works, nothing was consumed by the cancelled reads */
	CHECK(write(p[1], "abc", 3) == 3);
	CHECK(read(p[0], buf, sizeof buf) == 3);
	close(p[0]); close(p[1]); close(q[0]); close(q[1]);
}

static void listio(int fd)
{
	static char x[4][100];
	struct aiocb cb[5], *list[6];
	struct sigevent sev;
	sigset_t set;
	siginfo_t si;
	int i;
	memset(cb, 0, sizeof cb);
	for (i = 0; i < 4; i++) {
		memset(x[i], '0' + i, 100);
		cb[i].aio_fildes = fd;
		cb[i].aio_buf = x[i];
		cb[i].aio_nbytes = 100;
		cb[i].aio_offset = 100 * i;
		cb[i].aio_lio_opcode = LIO_WRITE;
		list[i] = &cb[i];
	}
	cb[2].aio_lio_opcode = LIO_NOP;
	list[4] = 0;
	cb[4] = cb[0];
	cb[4].aio_fildes = -1;
	list[5] = &cb[4];
	errno = 0;
	CHECK(lio_listio(LIO_WAIT, list, 6, 0) == -1 && errno == EIO);
	CHECK(aio_error(&cb[0]) == 0 && aio_return(&cb[0]) == 100);
	CHECK(aio_error(&cb[3]) == 0 && aio_return(&cb[3]) == 100);
	CHECK(aio_error(&cb[4]) == EBADF);
	CHECK(lio_listio(LIO_WAIT, list, 4, 0) == 0);
	/* LIO_NOWAIT with a signal when all are done */
	sigemptyset(&set);
	sigaddset(&set, SIGRTMIN + 2);
	pthread_sigmask(SIG_BLOCK, &set, 0);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN + 2;
	sev.sigev_value.sival_int = 99;
	for (i = 0; i < 4; i++)
		cb[i].aio_lio_opcode = LIO_READ;
	CHECK(lio_listio(LIO_NOWAIT, list, 4, &sev) == 0);
	CHECK(sigwaitinfo(&set, &si) == SIGRTMIN + 2);
	CHECK(si.si_value.sival_int == 99);
	for (i = 0; i < 4; i++)
		if (i != 2)
			CHECK(aio_error(&cb[i]) == 0 && aio_return(&cb[i]) == 100);
	/* LIO_NOWAIT: a bad descriptor fails alone; the list still completes */
	cb[4].aio_lio_opcode = LIO_READ;
	CHECK(lio_listio(LIO_NOWAIT, list, 6, &sev) == 0);
	CHECK(sigwaitinfo(&set, &si) == SIGRTMIN + 2);
	CHECK(aio_error(&cb[0]) == 0 && aio_return(&cb[0]) == 100);
	CHECK(aio_error(&cb[3]) == 0 && aio_return(&cb[3]) == 100);
	CHECK(aio_error(&cb[4]) == EBADF && aio_return(&cb[4]) == -1);
	errno = 0;
	CHECK(lio_listio(7, list, 4, 0) == -1 && errno == EINVAL);
	/* nothing to do: notification at once */
	CHECK(lio_listio(LIO_NOWAIT, list, 0, &sev) == 0);
	CHECK(sigwaitinfo(&set, &si) == SIGRTMIN + 2);
}

static void ordering(void)
{
	enum { N = 60 };
	static char blk[N][50], all[N * 50];
	struct aiocb cb[N], fs;
	int fd = open(path, O_RDWR | O_TRUNC | O_APPEND), i, ok = 1;
	for (i = 0; i < N; i++) {
		memset(blk[i], 'A' + i % 50, 50);
		memset(&cb[i], 0, sizeof cb[i]);
		cb[i].aio_fildes = fd;
		cb[i].aio_buf = blk[i];
		cb[i].aio_nbytes = 50;
		CHECK(aio_write(&cb[i]) == 0);
	}
	memset(&fs, 0, sizeof fs);
	fs.aio_fildes = fd;
	CHECK(aio_fsync(O_SYNC, &fs) == 0);
	waitfor(&fs);
	/* the fsync waited for all the writes before it */
	for (i = 0; i < N; i++)
		if (aio_error(&cb[i]) != 0 || aio_return(&cb[i]) != 50)
			ok = 0;
	CHECK(ok);
	CHECK(pread(fd, all, sizeof all, 0) == (ssize_t)sizeof all);
	for (i = 0; i < N; i++)
		if (memcmp(all + 50 * i, blk[i], 50))
			ok = 0;
	CHECK(ok);
	close(fd);
}

static void forked(int fd)
{
	int p[2], st;
	static char buf[16];
	struct aiocb cb;
	pid_t pid;
	CHECK(pipe(p) == 0);
	memset(&cb, 0, sizeof cb);
	cb.aio_fildes = p[0]; cb.aio_buf = buf; cb.aio_nbytes = sizeof buf;
	CHECK(aio_read(&cb) == 0);          /* pending in the parent */
	if (!(pid = fork())) {
		struct aiocb c2;
		memset(&c2, 0, sizeof c2);
		c2.aio_fildes = fd; c2.aio_buf = buf; c2.aio_nbytes = 10;
		if (aio_read(&c2)) _exit(1);
		waitfor(&c2);
		_exit(aio_return(&c2) == 10 && aio_cancel(p[0], 0) == AIO_ALLDONE ? 0 : 2);
	}
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(write(p[1], "z", 1) == 1);
	waitfor(&cb);
	CHECK(aio_return(&cb) == 1);
	close(p[0]); close(p[1]);
}

/* prioritized I/O: a request of a SCHED_FIFO thread runs at its priority
   less aio_reqprio (the notification function runs where it ran) */
static volatile int seen_pol = -1, seen_prio = -1;
static void prio_notify(union sigval v)
{
	struct sched_param sp;
	int pol;
	(void)v;
	if (pthread_getschedparam(pthread_self(), &pol, &sp) == 0)
		seen_pol = pol, seen_prio = sp.sched_priority;
	sem_post(&sem);
}

static void *prio_thread(void *p)
{
	int fd = *(int *)p;
	static char b[8];
	struct aiocb cb;
	memset(&cb, 0, sizeof cb);
	cb.aio_fildes = fd;
	cb.aio_buf = b;
	cb.aio_nbytes = 4;
	cb.aio_reqprio = 7;
	cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
	cb.aio_sigevent.sigev_notify_function = prio_notify;
	CHECK(aio_read(&cb) == 0);
	sem_wait(&sem);
	waitfor(&cb);
	CHECK(aio_return(&cb) == 4);
	return 0;
}

static void prioritized(int fd)
{
	pthread_attr_t a;
	pthread_t t;
	struct sched_param sp;
	CHECK(sysconf(_SC_PRIORITIZED_IO) > 0 && sysconf(_SC_AIO_PRIO_DELTA_MAX) == AIO_PRIO_DELTA_MAX);
	CHECK(fpathconf(fd, _PC_PRIO_IO) > 0);
	pthread_attr_init(&a);
	pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&a, SCHED_FIFO);
	sp.sched_priority = 30;
	pthread_attr_setschedparam(&a, &sp);
	if (pthread_create(&t, &a, prio_thread, &fd)) {
		os("prioritized I/O: no real-time threads allowed: not tested\n");
		return;
	}
	pthread_join(t, 0);
	CHECK(seen_pol == SCHED_FIFO && seen_prio == 23);
}

static void devctl(int fd)
{
	int p[2], n = -1, info = -1;
	CHECK(pipe(p) == 0);
	CHECK(write(p[1], "12345", 5) == 5);
	CHECK(posix_devctl(p[0], 0x541B /* FIONREAD */, &n, sizeof n, &info) == 0);
	CHECK(n == 5 && info == 0);
	CHECK(posix_devctl(-1, 0x541B, &n, sizeof n, 0) == EBADF);
	CHECK(posix_devctl(fd, 0x5413 /* TIOCGWINSZ */, &n, 8, 0) == ENOTTY);
	/* _IOR('T', 0x30, unsigned) needs 4 bytes */
	CHECK(posix_devctl(p[0], (int)0x80045430, &n, 2, 0) == EINVAL);
	close(p[0]); close(p[1]);
}

int main(void)
{
	int fd = mkstemp(path);
	CHECK(fd >= 0);
	basic(fd);
	notification(fd);
	pipes();
	listio(fd);
	ordering();
	fd = open(path, O_RDWR);
	forked(fd);
	prioritized(fd);
	devctl(fd);
	close(fd);
	unlink(path);
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}
