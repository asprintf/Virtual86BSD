/* Self-checking: mutex protocols PTHREAD_PRIO_INHERIT and
 * PTHREAD_PRIO_PROTECT (priority ceilings), robust or not.  The effective
 * priority of a thread is read from /proc/thread-self/stat (field 18,
 * -1 - rt_priority for real-time threads); the checks that need real-time
 * threads are skipped where they may not be created. */
#define _XOPEN_SOURCE 800
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "out.h"

/* the calling thread's effective priority, or 1000 */
static int eff_prio(void)
{
	char b[512], *p;
	int fd = open("/proc/thread-self/stat", O_RDONLY), n, f;
	ssize_t r;
	if (fd < 0) return 1000;
	r = read(fd, b, sizeof b - 1);
	close(fd);
	if (r <= 0) return 1000;
	b[r] = 0;
	if (!(p = strrchr(b, ')'))) return 1000;
	/* fields from 3 on follow the ")" */
	for (f = 3, p += 2; f < 18 && *p; p++) if (*p == ' ') f++;
	n = 0;
	{
		int neg = *p == '-';
		if (neg) p++;
		while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
		return neg ? -n : n;
	}
}

static void ms(long n)
{
	struct timespec ts = { n / 1000, n % 1000 * 1000000 };
	nanosleep(&ts, 0);
}

static pthread_mutex_t m;
static volatile int stage, seen;
static long counter;

static void *adder(void *a)
{
	int i;
	(void)a;
	for (i = 0; i < 20000; i++) {
		pthread_mutex_lock(&m);
		counter++;
		pthread_mutex_unlock(&m);
	}
	return 0;
}

static void *holder(void *a)
{
	(void)a;
	pthread_mutex_lock(&m);
	stage = 1;
	while (stage != 2) ms(1);
	pthread_mutex_unlock(&m);
	return 0;
}

static void *die_holding(void *a)
{
	(void)a;
	pthread_mutex_lock(&m);
	stage = 1;
	while (stage != 2) ms(1);
	return 0;   /* exits with the mutex */
}

static void *locker(void *a)
{
	(void)a;
	return (void *)(long)pthread_mutex_lock(&m);
}

/* a real-time thread at prio running f */
static int rt_thread(pthread_t *t, int prio, void *(*f)(void *), void *arg)
{
	pthread_attr_t at;
	struct sched_param sp = { .sched_priority = prio };
	int r;
	pthread_attr_init(&at);
	pthread_attr_setinheritsched(&at, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&at, SCHED_FIFO);
	pthread_attr_setschedparam(&at, &sp);
	r = pthread_create(t, &at, f, arg);
	pthread_attr_destroy(&at);
	return r;
}

/* PTHREAD_PRIO_INHERIT: the holder (prio 10) is raised to its waiter's 30 */
static void *pi_low(void *a)
{
	int i, boosted = 0;
	(void)a;
	pthread_mutex_lock(&m);
	stage = 1;
	for (i = 0; i < 2000 && !boosted; i++) {
		if (eff_prio() == -31) boosted = 1;
		else ms(1);
	}
	seen = boosted;
	pthread_mutex_unlock(&m);
	seen |= (eff_prio() == -11) << 1;   /* back to its own */
	return 0;
}

/* PTHREAD_PRIO_PROTECT at prio 10: ceilings raise it while held */
static pthread_mutex_t c20, c40;
static void *pp_thread(void *a)
{
	int r = 0;
	(void)a;
	r |= (pthread_mutex_lock(&c20) != 0) << 0;
	r |= (eff_prio() != -21) << 1;
	r |= (pthread_mutex_lock(&c40) != 0) << 2;
	r |= (eff_prio() != -41) << 3;
	r |= (pthread_mutex_unlock(&c40) != 0) << 4;
	r |= (eff_prio() != -21) << 5;
	r |= (pthread_mutex_unlock(&c20) != 0) << 6;
	r |= (eff_prio() != -11) << 7;
	return (void *)(long)r;
}

static void *pp_high(void *a)
{
	(void)a;
	return (void *)(long)pthread_mutex_lock(&c20);   /* own 50 > ceiling 20 */
}

static void init(int proto, int type, int robust)
{
	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_setprotocol(&ma, proto);
	pthread_mutexattr_settype(&ma, type);
	pthread_mutexattr_setrobust(&ma, robust);
	pthread_mutex_init(&m, &ma);
	pthread_mutexattr_destroy(&ma);
}

int main(void)
{
	pthread_mutexattr_t ma;
	pthread_t t[4];
	void *res;
	int i, v, rt;
	struct timespec ts;

	CHECK(sysconf(_SC_THREAD_PRIO_INHERIT) > 0 && sysconf(_SC_THREAD_PRIO_PROTECT) > 0);
	CHECK(sysconf(_SC_THREAD_ROBUST_PRIO_INHERIT) > 0 && sysconf(_SC_THREAD_ROBUST_PRIO_PROTECT) > 0);

	/* attributes */
	pthread_mutexattr_init(&ma);
	CHECK(pthread_mutexattr_getprotocol(&ma, &v) == 0 && v == PTHREAD_PRIO_NONE);
	CHECK(pthread_mutexattr_setprotocol(&ma, 7) == EINVAL);
	CHECK(pthread_mutexattr_setprotocol(&ma, PTHREAD_PRIO_INHERIT) == 0);
	CHECK(pthread_mutexattr_getprotocol(&ma, &v) == 0 && v == PTHREAD_PRIO_INHERIT);
	CHECK(pthread_mutexattr_getprioceiling(&ma, &v) == 0 && v == sched_get_priority_min(SCHED_FIFO));
	CHECK(pthread_mutexattr_setprioceiling(&ma, 0) == EINVAL && pthread_mutexattr_setprioceiling(&ma, 100) == EINVAL);
	CHECK(pthread_mutexattr_setprioceiling(&ma, 20) == 0);
	CHECK(pthread_mutexattr_getprioceiling(&ma, &v) == 0 && v == 20);
	pthread_mutexattr_destroy(&ma);

	/* trylock by the owner: EBUSY, not EDEADLK (also without a protocol) */
	init(PTHREAD_PRIO_NONE, PTHREAD_MUTEX_ERRORCHECK, 0);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_trylock(&m) == EBUSY && pthread_mutex_unlock(&m) == 0);
	init(PTHREAD_PRIO_NONE, PTHREAD_MUTEX_NORMAL, 1);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_trylock(&m) == EBUSY && pthread_mutex_unlock(&m) == 0);

	/* PTHREAD_PRIO_INHERIT, each type */
	init(PTHREAD_PRIO_INHERIT, PTHREAD_MUTEX_ERRORCHECK, 0);
	CHECK(pthread_mutex_getprioceiling(&m, &v) == EINVAL);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_lock(&m) == EDEADLK);
	CHECK(pthread_mutex_trylock(&m) == EBUSY);
	CHECK(pthread_mutex_unlock(&m) == 0 && pthread_mutex_unlock(&m) == EPERM);
	init(PTHREAD_PRIO_INHERIT, PTHREAD_MUTEX_RECURSIVE, 0);
	CHECK(pthread_mutex_lock(&m) == 0 && pthread_mutex_lock(&m) == 0 && pthread_mutex_trylock(&m) == 0);
	CHECK(pthread_mutex_unlock(&m) == 0 && pthread_mutex_unlock(&m) == 0 && pthread_mutex_unlock(&m) == 0);
	CHECK(pthread_mutex_unlock(&m) == EPERM);
	/* contention: the kernel's queue */
	init(PTHREAD_PRIO_INHERIT, PTHREAD_MUTEX_NORMAL, 0);
	counter = 0;
	for (i = 0; i < 4; i++) pthread_create(&t[i], 0, adder, 0);
	for (i = 0; i < 4; i++) pthread_join(t[i], 0);
	CHECK(counter == 80000);
	/* busy: trylock, and timeouts against both clocks */
	stage = 0;
	pthread_create(&t[0], 0, holder, 0);
	while (!stage) ms(1);
	CHECK(pthread_mutex_trylock(&m) == EBUSY);
	clock_gettime(CLOCK_REALTIME, &ts); ts.tv_nsec += 50000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_mutex_timedlock(&m, &ts) == ETIMEDOUT);
	clock_gettime(CLOCK_MONOTONIC, &ts); ts.tv_nsec += 50000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_mutex_clocklock(&m, CLOCK_MONOTONIC, &ts) == ETIMEDOUT);
	ts.tv_nsec = 1000000000;
	CHECK(pthread_mutex_timedlock(&m, &ts) == EINVAL);
	stage = 2;
	pthread_join(t[0], 0);
	CHECK(pthread_mutex_trylock(&m) == 0 && pthread_mutex_unlock(&m) == 0);
	/* a normal mutex locked again by its owner: blocks until the timeout */
	CHECK(pthread_mutex_lock(&m) == 0);
	clock_gettime(CLOCK_MONOTONIC, &ts); ts.tv_nsec += 20000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_mutex_clocklock(&m, CLOCK_MONOTONIC, &ts) == ETIMEDOUT);
	CHECK(pthread_mutex_unlock(&m) == 0);

	/* robust PI: the owner dies (uncontended, then with a waiter) */
	init(PTHREAD_PRIO_INHERIT, PTHREAD_MUTEX_NORMAL, 1);
	stage = 0;
	pthread_create(&t[0], 0, die_holding, 0);
	while (!stage) ms(1);
	stage = 2;
	pthread_join(t[0], 0);
	CHECK(pthread_mutex_lock(&m) == EOWNERDEAD);
	CHECK(pthread_mutex_consistent(&m) == 0 && pthread_mutex_unlock(&m) == 0);
	stage = 0;
	pthread_create(&t[0], 0, die_holding, 0);
	while (!stage) ms(1);
	pthread_create(&t[1], 0, locker, 0);
	ms(50);
	stage = 2;
	pthread_join(t[0], 0);
	pthread_join(t[1], &res);
	CHECK((long)res == EOWNERDEAD);
	/* t[1] died with it, inconsistent: the next owner sees EOWNERDEAD; left
	 * inconsistent, the mutex becomes unrecoverable, also for a waiter */
	CHECK(pthread_mutex_lock(&m) == EOWNERDEAD);
	pthread_create(&t[1], 0, locker, 0);
	ms(50);
	CHECK(pthread_mutex_unlock(&m) == 0);
	pthread_join(t[1], &res);
	CHECK((long)res == ENOTRECOVERABLE);
	CHECK(pthread_mutex_lock(&m) == ENOTRECOVERABLE && pthread_mutex_trylock(&m) == ENOTRECOVERABLE);

	/* PTHREAD_PRIO_PROTECT without real-time scheduling: no raise, no error */
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_setprotocol(&ma, PTHREAD_PRIO_PROTECT);
	pthread_mutexattr_setprioceiling(&ma, 20);
	pthread_mutex_init(&c20, &ma);
	pthread_mutexattr_setprioceiling(&ma, 40);
	pthread_mutex_init(&c40, &ma);
	pthread_mutexattr_destroy(&ma);
	CHECK(pthread_mutex_getprioceiling(&c20, &v) == 0 && v == 20);
	CHECK(pthread_mutex_lock(&c20) == 0 && pthread_mutex_unlock(&c20) == 0);
	CHECK(pthread_mutex_setprioceiling(&c20, 25, &v) == 0 && v == 20);
	CHECK(pthread_mutex_getprioceiling(&c20, &v) == 0 && v == 25);
	CHECK(pthread_mutex_setprioceiling(&c20, 20, 0) == 0 && pthread_mutex_setprioceiling(&c20, 0, 0) == EINVAL);
	CHECK(pthread_mutex_setprioceiling(&m, 20, 0) == EINVAL);   /* not PROTECT */

	/* with real-time threads, if allowed */
	rt = rt_thread(&t[0], 10, pp_thread, 0) == 0;
	if (rt) {
		pthread_join(t[0], &res);
		CHECK((long)res == 0);
		CHECK(rt_thread(&t[0], 50, pp_high, 0) == 0 && pthread_join(t[0], &res) == 0 && (long)res == EINVAL);
		init(PTHREAD_PRIO_INHERIT, PTHREAD_MUTEX_NORMAL, 0);
		stage = 0; seen = 0;
		CHECK(rt_thread(&t[0], 10, pi_low, 0) == 0);
		while (!stage) ms(1);
		CHECK(rt_thread(&t[1], 30, locker, 0) == 0);
		pthread_join(t[0], 0);
		pthread_join(t[1], &res);
		CHECK(seen == 3 && (long)res == 0);
	} else
		os("prio: real-time threads not allowed: boost checks skipped\n");

	os(failures ? "prio FAILED\n" : "prio ok\n");
	oflush();
	return failures != 0;
}
