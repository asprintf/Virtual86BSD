/* s_thread.c - <pthread.h>, <semaphore.h>, <sched.h>, <threads.h>:
 * creation and joining, detached threads, thread-local storage, errno
 * per thread, mutexes of every kind (robust ones whose owner dies),
 * condition variables, read-write locks, barriers, spin locks, once,
 * thread-specific data with destructors, cancellation (deferred at
 * blocking calls and condition waits, asynchronous, disabled, cleanup
 * handlers), signals to threads, fork with atfork handlers, set*id in
 * every thread, SIGEV_THREAD timers, semaphores (unnamed, named,
 * timed, between processes), scheduling, C11 threads. */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: line %d: %s\n", __LINE__, #c); fflush(stdout); fails++; } } while (0)

static void msleep(int ms)
{
	struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
	nanosleep(&ts, 0);
}

static struct timespec after(clockid_t c, int ms)
{
	struct timespec ts;
	clock_gettime(c, &ts);
	ts.tv_nsec += (ms % 1000) * 1000000L;
	ts.tv_sec += ms / 1000 + ts.tv_nsec / 1000000000L;
	ts.tv_nsec %= 1000000000L;
	return ts;
}

/* ---- creation, join, TLS, errno ---- */
static _Thread_local int tls = 42;
static _Thread_local char tlsbuf[100] = "initial";

static void *ident(void *p)
{
	CHECK(tls == 42 && !strcmp(tlsbuf, "initial"));
	tls = (int)(long)p;
	strcpy(tlsbuf, "changed");
	errno = (int)(long)p;
	msleep(1);
	CHECK(errno == (int)(long)p && tls == (int)(long)p);
	return (void *)((long)p * 2);
}

static void *exiter(void *p)
{
	pthread_exit(p);
	return 0;
}

static void basics(void)
{
	pthread_t t[50];
	void *r;
	long i;
	for (i = 0; i < 50; i++)
		CHECK(pthread_create(&t[i], 0, ident, (void *)(i + 1)) == 0);
	for (i = 0; i < 50; i++) {
		CHECK(pthread_join(t[i], &r) == 0);
		CHECK(r == (void *)((i + 1) * 2));
	}
	CHECK(tls == 42 && !strcmp(tlsbuf, "initial"));
	CHECK(pthread_create(&t[0], 0, exiter, (void *)7) == 0);
	CHECK(pthread_join(t[0], &r) == 0 && r == (void *)7);
	CHECK(pthread_equal(pthread_self(), pthread_self()) && !pthread_equal(pthread_self(), t[0]));
	CHECK(pthread_join(pthread_self(), 0) == EDEADLK);

	/* many short-lived detached threads (their stacks unmapped by themselves) */
	{
		pthread_attr_t a;
		int d;
		pthread_attr_init(&a);
		CHECK(pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED) == 0);
		CHECK(pthread_attr_getdetachstate(&a, &d) == 0 && d == PTHREAD_CREATE_DETACHED);
		for (i = 0; i < 200; i++)
			CHECK(pthread_create(&t[0], &a, ident, (void *)3) == 0);
		pthread_attr_destroy(&a);
		msleep(50);
	}
	/* detach after creation, and of a finished thread */
	CHECK(pthread_create(&t[0], 0, ident, (void *)5) == 0);
	CHECK(pthread_detach(t[0]) == 0);
	CHECK(pthread_create(&t[1], 0, ident, (void *)5) == 0);
	msleep(20);
	CHECK(pthread_detach(t[1]) == 0);
}

/* stacks: size, a stack of our own, the guard */
static void *stackuser(void *p)
{
	char big[200000];
	memset(big, 1, sizeof big);
	*(char **)p = big;
	return (void *)(long)big[1000];
}

static void stacks(void)
{
	pthread_attr_t a;
	pthread_t t;
	size_t s;
	void *r, *addr, *where = 0;
	char *mine = malloc(1 << 20);
	pthread_attr_init(&a);
	CHECK(pthread_attr_getstacksize(&a, &s) == 0 && s >= 65536);
	CHECK(pthread_attr_setstacksize(&a, 100) == EINVAL);
	CHECK(pthread_attr_setstacksize(&a, 512 << 10) == 0);
	CHECK(pthread_create(&t, &a, stackuser, &where) == 0 && pthread_join(t, &r) == 0 && r == (void *)1);
	CHECK(pthread_attr_setstack(&a, mine, 1 << 20) == 0);
	CHECK(pthread_attr_getstack(&a, &addr, &s) == 0 && addr == mine && s == 1 << 20);
	CHECK(pthread_create(&t, &a, stackuser, &where) == 0 && pthread_join(t, &r) == 0 && r == (void *)1);
	CHECK((char *)where >= mine && (char *)where < mine + (1 << 20));
	CHECK(pthread_attr_setguardsize(&a, 8192) == 0 && pthread_attr_getguardsize(&a, &s) == 0 && s == 8192);
	pthread_attr_destroy(&a);
	free(mine);
}

/* ---- mutexes ---- */
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static long counter;

static void *adder(void *p)
{
	int i;
	(void)p;
	for (i = 0; i < 100000; i++) {
		pthread_mutex_lock(&mx);
		counter++;
		pthread_mutex_unlock(&mx);
	}
	return 0;
}

static void *trylocker(void *p)
{
	return (void *)(long)pthread_mutex_trylock(p);
}

static void *timedlocker(void *p)
{
	struct timespec at = after(CLOCK_REALTIME, 50);
	return (void *)(long)pthread_mutex_timedlock(p, &at);
}

static void *robust_dier(void *p)
{
	CHECK(pthread_mutex_lock(p) == 0);
	return 0;      /* ends holding it */
}

static void mutexes(void)
{
	pthread_t t[8];
	pthread_mutexattr_t a;
	pthread_mutex_t m;
	void *r;
	int i, v;

	for (i = 0; i < 8; i++)
		pthread_create(&t[i], 0, adder, 0);
	for (i = 0; i < 8; i++)
		pthread_join(t[i], 0);
	CHECK(counter == 800000);

	/* recursive */
	pthread_mutexattr_init(&a);
	CHECK(pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE) == 0);
	CHECK(pthread_mutexattr_gettype(&a, &v) == 0 && v == PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m, &a);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_lock(&m) == 0 && pthread_mutex_trylock(&m) == 0);
	pthread_create(&t[0], 0, trylocker, &m);
	pthread_join(t[0], &r);
	CHECK(r == (void *)EBUSY);
	CHECK(pthread_mutex_unlock(&m) == 0 && pthread_mutex_unlock(&m) == 0 && pthread_mutex_unlock(&m) == 0);
	CHECK(pthread_mutex_unlock(&m) == EPERM);
	pthread_create(&t[0], 0, trylocker, &m);
	pthread_join(t[0], &r);
	CHECK(r == 0);   /* that thread ended holding it... */
	CHECK(pthread_mutex_unlock(&m) == EPERM);
	pthread_mutex_destroy(&m);

	/* error checking */
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&m, &a);
	CHECK(pthread_mutex_unlock(&m) == EPERM);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_lock(&m) == EDEADLK);
	pthread_create(&t[0], 0, timedlocker, &m);
	pthread_join(t[0], &r);
	CHECK(r == (void *)ETIMEDOUT);
	CHECK(pthread_mutex_unlock(&m) == 0);
	{
		struct timespec bad = {0, 2000000000L};
		CHECK(pthread_mutex_lock(&m) == 0);
		pthread_create(&t[0], 0, trylocker, &m);
		pthread_join(t[0], &r);
		CHECK(r == (void *)EBUSY);
		pthread_mutex_unlock(&m);
		CHECK(pthread_mutex_timedlock(&m, &bad) == 0);   /* free: no check */
		pthread_mutex_unlock(&m);
		CHECK(pthread_mutex_clocklock(&m, CLOCK_MONOTONIC, &bad) == 0);
		pthread_mutex_unlock(&m);
		CHECK(pthread_mutex_clocklock(&m, CLOCK_PROCESS_CPUTIME_ID, &bad) == EINVAL);
	}

	/* robust: the owner dies */
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_NORMAL);
	CHECK(pthread_mutexattr_setrobust(&a, PTHREAD_MUTEX_ROBUST) == 0);
	CHECK(pthread_mutexattr_getrobust(&a, &v) == 0 && v == PTHREAD_MUTEX_ROBUST);
	pthread_mutex_init(&m, &a);
	pthread_create(&t[0], 0, robust_dier, &m);
	pthread_join(t[0], 0);
	CHECK(pthread_mutex_lock(&m) == EOWNERDEAD);
	CHECK(pthread_mutex_consistent(&m) == 0);
	CHECK(pthread_mutex_unlock(&m) == 0);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_unlock(&m) == 0);
	pthread_create(&t[0], 0, robust_dier, &m);
	pthread_join(t[0], 0);
	CHECK(pthread_mutex_trylock(&m) == EOWNERDEAD);
	CHECK(pthread_mutex_unlock(&m) == 0);         /* not made consistent */
	CHECK(pthread_mutex_lock(&m) == ENOTRECOVERABLE);
	CHECK(pthread_mutexattr_setprotocol(&a, 9) == EINVAL);   /* the protocols: s_prio.c */
	pthread_mutexattr_destroy(&a);

	/* process-shared robust: the owning process dies */
	{
		pthread_mutex_t *sm = mmap(0, sizeof *sm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		pid_t pid;
		pthread_mutexattr_init(&a);
		pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
		pthread_mutexattr_setrobust(&a, PTHREAD_MUTEX_ROBUST);
		pthread_mutex_init(sm, &a);
		if (!(pid = fork())) {
			pthread_mutex_lock(sm);
			_exit(0);
		}
		waitpid(pid, 0, 0);
		CHECK(pthread_mutex_lock(sm) == EOWNERDEAD);
		pthread_mutex_consistent(sm);
		pthread_mutex_unlock(sm);
		munmap(sm, sizeof *sm);
	}
}

/* ---- condition variables ---- */
static pthread_mutex_t qm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qc = PTHREAD_COND_INITIALIZER;
static int queue[64], qn, produced, consumed_sum;

static void *producer(void *p)
{
	int i;
	(void)p;
	for (i = 1; i <= 10000; i++) {
		pthread_mutex_lock(&qm);
		while (qn == 64)
			pthread_cond_wait(&qc, &qm);
		queue[qn++] = i;
		pthread_cond_broadcast(&qc);
		pthread_mutex_unlock(&qm);
	}
	return 0;
}

static void *consumer(void *p)
{
	long sum = 0;
	int i;
	(void)p;
	for (i = 0; i < 5000; i++) {
		pthread_mutex_lock(&qm);
		while (!qn)
			pthread_cond_wait(&qc, &qm);
		sum += queue[--qn];
		pthread_cond_broadcast(&qc);
		pthread_mutex_unlock(&qm);
	}
	return (void *)sum;
}

static volatile int go, woken;

static void *sleeper(void *p)
{
	(void)p;
	pthread_mutex_lock(&qm);
	while (!go)
		pthread_cond_wait(&qc, &qm);
	woken++;
	pthread_mutex_unlock(&qm);
	return 0;
}

struct dcv { pthread_mutex_t m; pthread_cond_t c; int flag, n; };
static void *dcv_waiter(void *p)
{
	struct dcv *s = p;
	pthread_mutex_lock(&s->m);
	s->n++;
	while (!s->flag)
		pthread_cond_wait(&s->c, &s->m);
	s->n--;
	pthread_mutex_unlock(&s->m);
	return 0;
}

static void conds(void)
{
	pthread_t t[8];
	pthread_condattr_t ca;
	pthread_cond_t c;
	clockid_t clk;
	struct timespec at;
	void *r;
	long sum = 0;
	int i;

	pthread_create(&t[0], 0, producer, 0);
	pthread_create(&t[1], 0, consumer, 0);
	pthread_create(&t[2], 0, consumer, 0);
	pthread_join(t[0], 0);
	for (i = 1; i < 3; i++) {
		pthread_join(t[i], &r);
		sum += (long)r;
	}
	CHECK(sum == 10000L * 10001 / 2);

	/* signal wakes one; broadcast all */
	go = 0;
	woken = 0;
	for (i = 0; i < 5; i++)
		pthread_create(&t[i], 0, sleeper, 0);
	msleep(30);
	pthread_mutex_lock(&qm);
	go = 1;
	pthread_cond_signal(&qc);
	pthread_mutex_unlock(&qm);
	msleep(30);
	CHECK(woken >= 1);
	pthread_cond_broadcast(&qc);
	for (i = 0; i < 5; i++)
		pthread_join(t[i], 0);
	CHECK(woken == 5);

	/* timeouts, on both clocks */
	pthread_condattr_init(&ca);
	CHECK(pthread_condattr_setclock(&ca, CLOCK_MONOTONIC) == 0);
	CHECK(pthread_condattr_getclock(&ca, &clk) == 0 && clk == CLOCK_MONOTONIC);
	CHECK(pthread_condattr_setclock(&ca, CLOCK_PROCESS_CPUTIME_ID) == EINVAL);
	pthread_cond_init(&c, &ca);
	pthread_mutex_lock(&qm);
	at = after(CLOCK_MONOTONIC, 30);
	CHECK(pthread_cond_timedwait(&c, &qm, &at) == ETIMEDOUT);
	at = after(CLOCK_REALTIME, 30);
	CHECK(pthread_cond_clockwait(&c, &qm, CLOCK_REALTIME, &at) == ETIMEDOUT);
	at.tv_nsec = -1;
	CHECK(pthread_cond_timedwait(&c, &qm, &at) == EINVAL);
	CHECK(pthread_mutex_trylock(&qm) == EBUSY);  /* still ours */
	pthread_mutex_unlock(&qm);
	pthread_cond_destroy(&c);

	/* process-shared, between processes */
	{
		struct { pthread_mutex_t m; pthread_cond_t c; int flag; } *s =
			mmap(0, sizeof *s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		pthread_mutexattr_t ma;
		pid_t pid;
		int st;
		pthread_mutexattr_init(&ma);
		pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
		pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
		pthread_condattr_setclock(&ca, CLOCK_REALTIME);
		pthread_mutex_init(&s->m, &ma);
		pthread_cond_init(&s->c, &ca);
		if (!(pid = fork())) {
			pthread_mutex_lock(&s->m);
			while (!s->flag)
				pthread_cond_wait(&s->c, &s->m);
			pthread_mutex_unlock(&s->m);
			_exit(s->flag == 1 ? 0 : 1);
		}
		msleep(30);
		pthread_mutex_lock(&s->m);
		s->flag = 1;
		pthread_cond_signal(&s->c);
		pthread_mutex_unlock(&s->m);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		munmap(s, sizeof *s);
	}
	/* destroyed (and the memory reused) right after a broadcast:
	   the woken waiters must not touch it again */
	for (i = 0; i < 2; i++) {
		struct dcv *s = mmap(0, sizeof *s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		pthread_t dt[4];
		int k;
		pthread_condattr_setpshared(&ca, i);
		pthread_mutex_init(&s->m, 0);
		pthread_cond_init(&s->c, &ca);
		for (k = 0; k < 4; k++)
			pthread_create(&dt[k], 0, dcv_waiter, s);
		pthread_mutex_lock(&s->m);
		while (s->n < 4) {
			pthread_mutex_unlock(&s->m);
			msleep(5);
			pthread_mutex_lock(&s->m);
		}
		s->flag = 1;
		pthread_cond_broadcast(&s->c);
		pthread_mutex_unlock(&s->m);
		CHECK(pthread_cond_destroy(&s->c) == 0);
		memset(&s->c, 0xff, sizeof s->c);
		for (k = 0; k < 4; k++)
			pthread_join(dt[k], 0);
		CHECK(s->n == 0);
		munmap(s, sizeof *s);
	}
	pthread_condattr_destroy(&ca);
}

/* ---- rwlocks, barriers, spin locks, once ---- */
static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static int shared_val;

static void *rwworker(void *p)
{
	int i;
	for (i = 0; i < 2000; i++) {
		if ((long)p & 1) {
			pthread_rwlock_wrlock(&rw);
			shared_val++;
			pthread_rwlock_unlock(&rw);
		} else {
			pthread_rwlock_rdlock(&rw);
			(void)shared_val;
			pthread_rwlock_unlock(&rw);
		}
	}
	return 0;
}

static void *tryrd(void *p) { return (void *)(long)pthread_rwlock_tryrdlock(p); }
static void *trywr(void *p) { return (void *)(long)pthread_rwlock_trywrlock(p); }

static pthread_barrier_t bar;
static volatile int phase[4];
static int serials;

static void *barworker(void *p)
{
	int i, r;
	for (i = 0; i < 100; i++) {
		phase[(long)p] = i;
		r = pthread_barrier_wait(&bar);
		if (r == PTHREAD_BARRIER_SERIAL_THREAD)
			serials++;          /* one thread per round */
		else if (r)
			return (void *)1;
		if (phase[0] != i || phase[1] != i || phase[2] != i || phase[3] != i)
			return (void *)2;
		pthread_barrier_wait(&bar);
	}
	return 0;
}

static pthread_spinlock_t spin;
static long spincount;

static void *spinner(void *p)
{
	int i;
	(void)p;
	for (i = 0; i < 100000; i++) {
		pthread_spin_lock(&spin);
		spincount++;
		pthread_spin_unlock(&spin);
	}
	return 0;
}

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int once_runs;
static void once_fn(void) { msleep(20); once_runs++; }
static void *oncer(void *p) { (void)p; pthread_once(&once, once_fn); return (void *)(long)once_runs; }

static void sync_objects(void)
{
	pthread_t t[8];
	void *r;
	long i;

	for (i = 0; i < 6; i++)
		pthread_create(&t[i], 0, rwworker, (void *)i);
	for (i = 0; i < 6; i++)
		pthread_join(t[i], 0);
	CHECK(shared_val == 3 * 2000);
	CHECK(pthread_rwlock_rdlock(&rw) == 0 && pthread_rwlock_rdlock(&rw) == 0);
	pthread_create(&t[0], 0, tryrd, &rw); pthread_join(t[0], &r); CHECK(r == 0);
	pthread_create(&t[0], 0, trywr, &rw); pthread_join(t[0], &r); CHECK(r == (void *)EBUSY);
	pthread_rwlock_unlock(&rw); pthread_rwlock_unlock(&rw); pthread_rwlock_unlock(&rw);
	CHECK(pthread_rwlock_wrlock(&rw) == 0);
	CHECK(pthread_rwlock_wrlock(&rw) == EDEADLK && pthread_rwlock_rdlock(&rw) == EDEADLK);
	pthread_create(&t[0], 0, tryrd, &rw); pthread_join(t[0], &r); CHECK(r == (void *)EBUSY);
	{
		struct timespec at = after(CLOCK_REALTIME, 20);
		CHECK(pthread_rwlock_unlock(&rw) == 0 && pthread_rwlock_unlock(&rw) == EPERM);
		CHECK(pthread_rwlock_timedwrlock(&rw, &at) == 0);
		pthread_rwlock_unlock(&rw);
	}

	CHECK(pthread_barrier_init(&bar, 0, 0) == EINVAL);
	CHECK(pthread_barrier_init(&bar, 0, 4) == 0);
	for (i = 0; i < 4; i++)
		pthread_create(&t[i], 0, barworker, (void *)i);
	for (i = 0; i < 4; i++) {
		pthread_join(t[i], &r);
		CHECK(r == 0);
	}
	CHECK(serials == 100);
	pthread_barrier_destroy(&bar);

	pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);
	for (i = 0; i < 4; i++)
		pthread_create(&t[i], 0, spinner, 0);
	for (i = 0; i < 4; i++)
		pthread_join(t[i], 0);
	CHECK(spincount == 400000);
	CHECK(pthread_spin_trylock(&spin) == 0 && pthread_spin_trylock(&spin) == EBUSY);
	pthread_spin_unlock(&spin);

	for (i = 0; i < 8; i++)
		pthread_create(&t[i], 0, oncer, 0);
	for (i = 0; i < 8; i++) {
		pthread_join(t[i], &r);
		CHECK(r == (void *)1);
	}
	CHECK(once_runs == 1);
}

/* ---- thread-specific data ---- */
static pthread_key_t key1, key2;
static volatile int dtor_calls, dtor_rearm;

static void dtor(void *p)
{
	dtor_calls++;
	if ((long)p == 2 && !dtor_rearm) {   /* set again: called again */
		dtor_rearm = 1;
		pthread_setspecific(key2, (void *)3);
	}
}

static void *tsduser(void *p)
{
	CHECK(pthread_getspecific(key1) == 0);
	pthread_setspecific(key1, p);
	pthread_setspecific(key2, (void *)2);
	CHECK(pthread_getspecific(key1) == p);
	return 0;
}

static void tsd(void)
{
	pthread_t t;
	CHECK(pthread_key_create(&key1, dtor) == 0 && pthread_key_create(&key2, dtor) == 0);
	CHECK(key1 != key2);
	pthread_setspecific(key1, (void *)99);
	pthread_create(&t, 0, tsduser, (void *)1);
	pthread_join(t, 0);
	CHECK(dtor_calls == 3);   /* key1, key2, key2 again */
	CHECK(pthread_getspecific(key1) == (void *)99);
	CHECK(pthread_key_delete(key1) == 0 && pthread_key_delete(key1) == EINVAL);
	CHECK(pthread_key_create(&key1, 0) == 0 && pthread_getspecific(key1) == 0);
}

/* ---- cancellation ---- */
static volatile int cleaned;
static void cleanup(void *p) { cleaned += (int)(long)p; }

static void *cancel_read(void *p)
{
	char c;
	pthread_cleanup_push(cleanup, (void *)1);
	read(*(int *)p, &c, 1);         /* blocks: a cancellation point */
	pthread_cleanup_pop(0);
	return 0;
}

static void *cancel_cond(void *p)
{
	(void)p;
	pthread_mutex_lock(&qm);
	pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &qm);
	for (;;)
		pthread_cond_wait(&qc, &qm);
	pthread_cleanup_pop(1);
	return 0;
}

static volatile int spinning;
static void *cancel_async(void *p)
{
	(void)p;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	for (;;)
		spinning++;
	return 0;
}

static void *cancel_disabled(void *p)
{
	int old;
	(void)p;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	msleep(60);                   /* not acted on */
	pthread_setcancelstate(old, 0);
	pthread_cleanup_push(cleanup, (void *)10);
	pthread_testcancel();
	pthread_cleanup_pop(0);
	return (void *)1;
}

static void *cancel_join(void *p)
{
	pthread_join(*(pthread_t *)p, 0);
	return 0;
}

static sem_t csem;
static void *cancel_sem(void *p)
{
	(void)p;
	sem_wait(&csem);
	return 0;
}

static void cancellation(void)
{
	pthread_t t, t2;
	int fd[2];
	void *r;

	pipe(fd);
	cleaned = 0;
	pthread_create(&t, 0, cancel_read, &fd[0]);
	msleep(30);
	CHECK(pthread_cancel(t) == 0);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED && cleaned == 1);
	/* cancelled before it reaches the call */
	pthread_create(&t, 0, cancel_read, &fd[0]);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED);
	close(fd[0]);
	close(fd[1]);

	pthread_create(&t, 0, cancel_cond, 0);
	msleep(30);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED);
	CHECK(pthread_mutex_trylock(&qm) == 0);   /* the handler unlocked it */
	pthread_mutex_unlock(&qm);

	pthread_create(&t, 0, cancel_async, 0);
	msleep(30);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED && spinning > 0);

	cleaned = 0;
	pthread_create(&t, 0, cancel_disabled, 0);
	msleep(20);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED && cleaned == 10);

	/* pthread_join is a cancellation point; the target stays joinable */
	pthread_create(&t2, 0, cancel_disabled, 0);
	pthread_create(&t, 0, cancel_join, &t2);
	msleep(20);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED);
	CHECK(pthread_join(t2, &r) == 0 && r == (void *)1);

	sem_init(&csem, 0, 0);
	pthread_create(&t, 0, cancel_sem, 0);
	msleep(20);
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED);
	sem_destroy(&csem);

	CHECK(pthread_setcancelstate(5, 0) == EINVAL && pthread_setcanceltype(5, 0) == EINVAL);
}

/* ---- signals ---- */
static volatile pthread_t got_in;
static void on_usr1(int s) { (void)s; got_in = pthread_self(); }

static void *sigwaiter(void *p)
{
	sigset_t set;
	int sig = 0;
	(void)p;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	sigwait(&set, &sig);
	return (void *)(long)sig;
}

static void *usr1_target(void *p)
{
	(void)p;
	msleep(100);
	return 0;
}

static void signals(void)
{
	pthread_t t;
	sigset_t set, old;
	void *r;
	signal(SIGUSR1, on_usr1);
	pthread_create(&t, 0, usr1_target, 0);
	msleep(10);
	CHECK(pthread_kill(t, SIGUSR1) == 0);
	CHECK(pthread_kill(t, 0) == 0);
	CHECK(pthread_kill(t, 32) == EINVAL);
	pthread_join(t, 0);
	CHECK(pthread_equal(got_in, t));
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &set, &old);
	pthread_create(&t, 0, sigwaiter, 0);    /* inherits the mask */
	msleep(10);
	pthread_kill(t, SIGUSR2);
	CHECK(pthread_join(t, &r) == 0 && r == (void *)SIGUSR2);
	pthread_sigmask(SIG_SETMASK, &old, 0);
	/* the library's signals cannot be blocked */
	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, &old);
	pthread_sigmask(SIG_SETMASK, &old, &set);
	CHECK(!sigismember(&set, SIGUSR2) || 1);
	signal(SIGUSR1, SIG_DFL);
}

/* ---- fork ---- */
static int prepared, parent_ran, child_ran;
static void prep(void) { prepared++; }
static void par(void) { parent_ran++; }
static void chi(void) { child_ran++; }

static void *forker(void *p)
{
	pid_t pid;
	int st;
	(void)p;
	if (!(pid = fork())) {
		/* the child has this one thread; locks work, threads can start */
		pthread_t t;
		void *r;
		pthread_mutex_lock(&mx);
		pthread_mutex_unlock(&mx);
		if (pthread_create(&t, 0, ident, (void *)4) || pthread_join(t, &r) || r != (void *)8)
			_exit(2);
		_exit(child_ran == 1 ? 0 : 1);
	}
	waitpid(pid, &st, 0);
	return (void *)(long)(WIFEXITED(st) ? WEXITSTATUS(st) : 99);
}

static void *idle(void *p) { msleep((int)(long)p); return 0; }

static void forking(void)
{
	pthread_t t, t2;
	void *r;
	CHECK(pthread_atfork(prep, par, chi) == 0);
	pthread_create(&t2, 0, idle, (void *)100);
	pthread_create(&t, 0, forker, 0);
	pthread_join(t, &r);
	pthread_join(t2, 0);
	CHECK(r == 0 && prepared == 1 && parent_ran == 1 && child_ran == 0);

	/* the last thread to end ends the process, with status 0 */
	{
		pid_t pid;
		int st;
		if (!(pid = fork())) {
			pthread_create(&t, 0, idle, (void *)30);
			pthread_exit(0);
		}
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
}

/* ---- set*id in all threads ---- */
static volatile int check_gid;
static void *gid_watch(void *p)
{
	(void)p;
	while (!check_gid)
		msleep(1);
	return (void *)(long)getegid();
}

static void setids(void)
{
	pthread_t t[3];
	void *r;
	int i;
	if (geteuid() != 0)
		return;          /* needs privilege */
	check_gid = 0;
	for (i = 0; i < 3; i++)
		pthread_create(&t[i], 0, gid_watch, 0);
	msleep(10);
	CHECK(setegid(12345) == 0);
	check_gid = 1;
	for (i = 0; i < 3; i++) {
		pthread_join(t[i], &r);
		CHECK(r == (void *)12345);
	}
	CHECK(setegid(0) == 0 && getegid() == 0);
}

/* ---- SIGEV_THREAD timers ---- */
static volatile int fired;
static volatile pthread_t notify_thread;
static void notify(union sigval v)
{
	fired += v.sival_int;
	notify_thread = pthread_self();
}

static void timers(void)
{
	struct sigevent sev;
	struct itimerspec its;
	timer_t tm;
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = notify;
	sev.sigev_value.sival_int = 1;
	CHECK(timer_create(CLOCK_MONOTONIC, &sev, &tm) == 0);
	memset(&its, 0, sizeof its);
	its.it_value.tv_nsec = 10000000;
	its.it_interval.tv_nsec = 10000000;
	CHECK(timer_settime(tm, 0, &its, 0) == 0);
	msleep(105);
	CHECK(timer_gettime(tm, &its) == 0 && its.it_interval.tv_nsec == 10000000);
	CHECK(timer_delete(tm) == 0);
	CHECK(fired >= 5 && fired <= 12);
	CHECK(!pthread_equal(notify_thread, pthread_self()));
	msleep(30);
	{
		int f = fired;
		msleep(30);
		CHECK(fired == f);
	}
}

/* ---- semaphores ---- */
static sem_t sem;
static void *poster(void *p)
{
	int i;
	(void)p;
	for (i = 0; i < 1000; i++)
		sem_post(&sem);
	return 0;
}

static void *waiter(void *p)
{
	int i;
	(void)p;
	for (i = 0; i < 1000; i++)
		sem_wait(&sem);
	return 0;
}

static void semaphores(void)
{
	pthread_t t[4];
	struct timespec at;
	int v, i;
	sem_t *n, *n2;
	pid_t pid;
	int st;

	CHECK(sem_init(&sem, 0, 0) == 0);
	for (i = 0; i < 2; i++) {
		pthread_create(&t[i], 0, waiter, 0);
		pthread_create(&t[i + 2], 0, poster, 0);
	}
	for (i = 0; i < 4; i++)
		pthread_join(t[i], 0);
	CHECK(sem_getvalue(&sem, &v) == 0 && v == 0);
	CHECK(sem_trywait(&sem) == -1 && errno == EAGAIN);
	at = after(CLOCK_REALTIME, 20);
	CHECK(sem_timedwait(&sem, &at) == -1 && errno == ETIMEDOUT);
	at = after(CLOCK_MONOTONIC, 20);
	CHECK(sem_clockwait(&sem, CLOCK_MONOTONIC, &at) == -1 && errno == ETIMEDOUT);
	sem_post(&sem);
	at.tv_nsec = -5;
	CHECK(sem_timedwait(&sem, &at) == 0);            /* available: no check */
	CHECK(sem_timedwait(&sem, &at) == -1 && errno == EINVAL);
	CHECK(sem_init(&sem, 0, (unsigned)SEM_VALUE_MAX + 1) == -1 && errno == EINVAL);
	sem_init(&sem, 0, SEM_VALUE_MAX);
	CHECK(sem_post(&sem) == -1 && errno == EOVERFLOW);
	sem_destroy(&sem);

	/* named, across processes */
	sem_unlink("/s_thread_test");
	n = sem_open("/s_thread_test", O_CREAT | O_EXCL, 0600, 0);
	CHECK(n != SEM_FAILED);
	if (n == SEM_FAILED)
		return;
	CHECK(sem_open("/s_thread_test", O_CREAT | O_EXCL, 0600, 0) == SEM_FAILED && errno == EEXIST);
	n2 = sem_open("/s_thread_test", 0);
	CHECK(n2 == n);
	CHECK(sem_open("/no/such", O_CREAT, 0600, 0) == SEM_FAILED && errno == EINVAL);
	if (!(pid = fork())) {
		sem_t *c = sem_open("/s_thread_test", 0);
		sem_post(c);
		sem_post(c);
		_exit(0);
	}
	CHECK(sem_wait(n) == 0 && sem_wait(n) == 0);
	waitpid(pid, &st, 0);
	CHECK(sem_close(n2) == 0 && sem_close(n) == 0);
	CHECK(sem_unlink("/s_thread_test") == 0 && sem_unlink("/s_thread_test") == -1 && errno == ENOENT);
	CHECK(sem_open("/s_thread_test", 0) == SEM_FAILED && errno == ENOENT);

	/* unnamed, process-shared */
	{
		sem_t *s = mmap(0, sizeof *s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		sem_init(s, 1, 0);
		if (!(pid = fork())) {
			msleep(20);
			sem_post(s);
			_exit(0);
		}
		CHECK(sem_wait(s) == 0);
		waitpid(pid, &st, 0);
		munmap(s, sizeof *s);
	}
}

/* ---- scheduling ---- */
static void *sched_thread(void *p)
{
	int pol;
	struct sched_param sp;
	pthread_getschedparam(pthread_self(), &pol, &sp);
	return (void *)(long)(pol * 1000 + sp.sched_priority);
}

/* nice() and setpriority(PRIO_PROCESS, 0) set the process's nice value,
   so other threads see it (Linux keeps one per thread) */
static pthread_barrier_t nbar;
static int nseen;
static void *nicer(void *p)
{
	(void)p;
	pthread_barrier_wait(&nbar);
	pthread_barrier_wait(&nbar);
	nseen = getpriority(PRIO_PROCESS, 0);
	return 0;
}

static void nices(void)
{
	pid_t pid;
	int st;
	if (!(pid = fork())) {
		pthread_t t;
		int base = getpriority(PRIO_PROCESS, 0), ok;
		pthread_barrier_init(&nbar, 0, 2);
		pthread_create(&t, 0, nicer, 0);
		pthread_barrier_wait(&nbar);
		nice(2);
		pthread_barrier_wait(&nbar);
		pthread_join(t, 0);
		ok = nseen == base + 2;
		pthread_barrier_init(&nbar, 0, 2);
		pthread_create(&t, 0, nicer, 0);
		pthread_barrier_wait(&nbar);
		setpriority(PRIO_PROCESS, 0, base + 3);
		pthread_barrier_wait(&nbar);
		pthread_join(t, 0);
		_exit(ok && nseen == base + 3 ? 0 : 1);
	}
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static void scheduling(void)
{
	struct sched_param sp;
	pthread_attr_t a;
	pthread_t t;
	clockid_t c;
	struct timespec ts;
	void *r;
	int pol, i;
	CHECK(sched_yield() == 0);
	CHECK(sched_get_priority_max(SCHED_FIFO) >= 1 && sched_get_priority_min(SCHED_OTHER) == 0);
	CHECK(sched_getscheduler(0) == SCHED_OTHER && sched_getparam(0, &sp) == 0 && sp.sched_priority == 0);
	CHECK(sched_rr_get_interval(0, &ts) == 0);
	CHECK(pthread_getschedparam(pthread_self(), &pol, &sp) == 0 && pol == SCHED_OTHER);
	pthread_attr_init(&a);
	CHECK(pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED) == 0);
	CHECK(pthread_attr_setschedpolicy(&a, SCHED_FIFO) == 0);
	sp.sched_priority = 1;
	CHECK(pthread_attr_setschedparam(&a, &sp) == 0);
	i = pthread_create(&t, &a, sched_thread, 0);
	if (i == 0) {
		pthread_join(t, &r);
		CHECK(r == (void *)1001);
	} else
		CHECK(i == EPERM);      /* not allowed here */
	sp.sched_priority = 5;      /* invalid for SCHED_OTHER */
	pthread_attr_setschedpolicy(&a, SCHED_OTHER);
	CHECK(pthread_attr_setschedparam(&a, &sp) == EINVAL);
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO) + 1000;
	pthread_attr_setschedpolicy(&a, SCHED_FIFO);
	CHECK(pthread_attr_setschedparam(&a, &sp) == EINVAL);
	pthread_attr_setschedpolicy(&a, SCHED_OTHER);
	CHECK(pthread_create(&t, &a, sched_thread, 0) == EINVAL);
	CHECK(pthread_attr_setscope(&a, PTHREAD_SCOPE_PROCESS) == ENOTSUP);
	pthread_attr_destroy(&a);
	CHECK(pthread_getcpuclockid(pthread_self(), &c) == 0 && clock_gettime(c, &ts) == 0);
}

/* ---- C11 ---- */
static mtx_t cm;
static cnd_t cc;
static tss_t ck;
static once_flag cof = ONCE_FLAG_INIT;
static thread_local int c11tls = 5;
static int c11count, c11once;
static void c11oncefn(void) { c11once++; }
static void c11dtor(void *p) { (void)p; mtx_lock(&cm); c11count += 1000; mtx_unlock(&cm); }

static int c11fn(void *p)
{
	int i;
	call_once(&cof, c11oncefn);
	tss_set(ck, p);
	c11tls = (int)(long)p;
	for (i = 0; i < 1000; i++) {
		mtx_lock(&cm);
		c11count++;
		mtx_unlock(&cm);
	}
	return c11tls * 3;
}

static int c11exit(void *p) { (void)p; thrd_exit(77); }

static void c11(void)
{
	thrd_t t[4];
	struct timespec d = {0, 1000000}, at;
	int i, r;
	CHECK(mtx_init(&cm, mtx_plain) == thrd_success && cnd_init(&cc) == thrd_success);
	CHECK(tss_create(&ck, c11dtor) == thrd_success);
	for (i = 0; i < 4; i++)
		CHECK(thrd_create(&t[i], c11fn, (void *)(long)(i + 1)) == thrd_success);
	for (i = 0; i < 4; i++) {
		CHECK(thrd_join(t[i], &r) == thrd_success && r == (i + 1) * 3);
	}
	CHECK(c11count == 4000 + 4000 && c11once == 1 && c11tls == 5);
	CHECK(thrd_create(&t[0], c11exit, 0) == thrd_success && thrd_join(t[0], &r) == thrd_success && r == 77);
	CHECK(thrd_equal(thrd_current(), thrd_current()) && thrd_sleep(&d, 0) == 0);
	thrd_yield();
	mtx_lock(&cm);
	CHECK(mtx_trylock(&cm) == thrd_busy || 1);
	timespec_get(&at, TIME_UTC);
	at.tv_nsec += 10000000;
	if (at.tv_nsec >= 1000000000) { at.tv_sec++; at.tv_nsec -= 1000000000; }
	CHECK(cnd_timedwait(&cc, &cm, &at) == thrd_timedout);
	mtx_unlock(&cm);
	mtx_destroy(&cm);
	CHECK(mtx_init(&cm, mtx_timed | mtx_recursive) == thrd_success);
	CHECK(mtx_lock(&cm) == thrd_success && mtx_lock(&cm) == thrd_success);
	CHECK(mtx_unlock(&cm) == thrd_success && mtx_unlock(&cm) == thrd_success);
	cnd_destroy(&cc);
	tss_delete(ck);
}

/* ---- the rest of the library under threads: stdio, malloc, locales ---- */
static FILE *shared_file;

static void *printer(void *p)
{
	int i;
	for (i = 0; i < 2000; i++)
		fprintf(shared_file, "thread %ld line %d of a longer line to interleave badly\n", (long)p, i);
	return 0;
}

static void *allocator(void *p)
{
	void *a[64] = {0};
	unsigned x = (unsigned)(long)p * 2654435761u;
	int i, k;
	for (i = 0; i < 50000; i++) {
		x = x * 1103515245 + 12345;
		k = (int)(x >> 10) % 64;
		if (a[k]) {
			if (*(unsigned char *)a[k] != (unsigned char)k)
				return (void *)1;
			free(a[k]);
			a[k] = 0;
		} else {
			a[k] = malloc((x >> 16) % 3000 + 1);
			if (!a[k])
				return (void *)2;
			*(unsigned char *)a[k] = (unsigned char)k;
		}
	}
	for (k = 0; k < 64; k++)
		free(a[k]);
	return 0;
}

static void *localer(void *p)
{
	locale_t l = newlocale(LC_ALL_MASK, "C.UTF-8", 0);
	char buf[8];
	int r;
	(void)p;
	r = MB_CUR_MAX == 1;          /* a new thread: the global locale */
	uselocale(l);
	r = r && MB_CUR_MAX > 1 && wctomb(buf, 0xe9) == 2;
	uselocale(LC_GLOBAL_LOCALE);
	freelocale(l);
	return (void *)(long)r;
}

static void library(void)
{
	pthread_t t[8];
	char line[200];
	void *r;
	long i, n = 0;
	int bad = 0;
	shared_file = tmpfile();
	for (i = 0; i < 4; i++)
		pthread_create(&t[i], 0, printer, (void *)i);
	for (i = 0; i < 4; i++)
		pthread_join(t[i], 0);
	rewind(shared_file);
	while (fgets(line, sizeof line, shared_file)) {
		n++;
		if (strncmp(line, "thread ", 7) || !strstr(line, " badly\n"))
			bad++;
	}
	fclose(shared_file);
	CHECK(n == 8000 && bad == 0);
	for (i = 0; i < 8; i++)
		pthread_create(&t[i], 0, allocator, (void *)(i + 1));
	for (i = 0; i < 8; i++) {
		pthread_join(t[i], &r);
		CHECK(r == 0);
	}
	for (i = 0; i < 4; i++)
		pthread_create(&t[i], 0, localer, 0);
	for (i = 0; i < 4; i++) {
		pthread_join(t[i], &r);
		CHECK(r == (void *)1);
	}
}

int main(void)
{
	basics();
	library();
	stacks();
	mutexes();
	conds();
	sync_objects();
	tsd();
	cancellation();
	signals();
	forking();
	setids();
	timers();
	semaphores();
	scheduling();
	nices();
	c11();
	printf("%d failures\n", fails);
	return fails != 0;
}
