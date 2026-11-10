/* Self-checking: the sporadic server (SCHED_SPORADIC, [SS] and [TSP]),
 * where the kernel has it (V86BSD's: sysconf says so; Linux: not tested).
 * A server with a budget of 20 ms every 100 ms, competing with a busy
 * SCHED_FIFO thread whose priority lies between the server's two, gets
 * about a fifth of the processor; as plain SCHED_FIFO it would get all
 * of it.  Needs permission for real-time priorities (root). */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "out.h"

static volatile int stop;
static volatile long spins[2];

static void *busy(void *p)
{
	long i = (long)p;
	while (!stop)
		spins[i]++;
	return 0;
}

static double cpu(pthread_t t)
{
	clockid_t c;
	struct timespec ts;
	if (pthread_getcpuclockid(t, &c) || clock_gettime(c, &ts))
		return -1;
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static struct sched_param ss(int hi, int lo, long budget_ms, long period_ms, int repl)
{
	struct sched_param p;
	memset(&p, 0, sizeof p);
	p.sched_priority = hi;
	p.sched_ss_low_priority = lo;
	p.sched_ss_init_budget.tv_sec = budget_ms / 1000;
	p.sched_ss_init_budget.tv_nsec = budget_ms % 1000 * 1000000;
	p.sched_ss_repl_period.tv_sec = period_ms / 1000;
	p.sched_ss_repl_period.tv_nsec = period_ms % 1000 * 1000000;
	p.sched_ss_max_repl = repl;
	return p;
}

int main(void)
{
	struct sched_param p, q;
	pthread_attr_t a;
	pthread_t ts, tf;
	int pol;

	if (sysconf(_SC_SPORADIC_SERVER) < 0) {
		CHECK(sysconf(_SC_THREAD_SPORADIC_SERVER) < 0 && sysconf(_SC_SS_REPL_MAX) < 0);
		pthread_attr_init(&a);
		CHECK(pthread_attr_setschedpolicy(&a, SCHED_SPORADIC) == ENOTSUP);
		os("sporadic server: not supported here: not tested\n");
		os(failures ? "failures\n" : "ok\n");
		oflush();
		return failures != 0;
	}
	CHECK(sysconf(_SC_THREAD_SPORADIC_SERVER) == 202405L && sysconf(_SC_SS_REPL_MAX) >= 4);
	CHECK(sched_get_priority_min(SCHED_SPORADIC) == 1 && sched_get_priority_max(SCHED_SPORADIC) >= 32);

	/* the process: set, read back, errors */
	p = ss(20, 5, 10, 50, 3);
	if (sched_setscheduler(0, SCHED_SPORADIC, &p) < 0 && errno == EPERM) {
		os("sporadic server: no real-time priorities allowed: not tested\n");
		oflush();
		return 0;
	}
	CHECK(sched_getscheduler(0) == SCHED_SPORADIC);
	memset(&q, 0x55, sizeof q);
	CHECK(sched_getparam(0, &q) == 0 && q.sched_priority == 20 && q.sched_ss_low_priority == 5 &&
	      q.sched_ss_init_budget.tv_nsec == 10000000 && q.sched_ss_repl_period.tv_nsec == 50000000 &&
	      q.sched_ss_max_repl == 3);
	p = ss(20, 5, 60, 50, 3);                       /* budget beyond the period */
	errno = 0;
	CHECK(sched_setparam(0, &p) == -1 && errno == EINVAL);
	p = ss(20, 5, 10, 50, 0);                       /* no replenishments */
	errno = 0;
	CHECK(sched_setparam(0, &p) == -1 && errno == EINVAL);
	/* a priority change keeps the server's other parameters */
	CHECK(pthread_setschedprio(pthread_self(), 25) == 0);
	CHECK(pthread_getschedparam(pthread_self(), &pol, &q) == 0 && pol == SCHED_SPORADIC && q.sched_priority == 25 &&
	      q.sched_ss_max_repl == 3);
	/* the parent itself as a high-priority FIFO thread, to keep control */
	p.sched_priority = 90;
	CHECK(sched_setscheduler(0, SCHED_FIFO, &p) >= 0);

	/* spawn attributes keep all of it */
	{
		posix_spawnattr_t sa;
		posix_spawnattr_init(&sa);
		p = ss(30, 2, 7, 70, 4);
		CHECK(posix_spawnattr_setschedpolicy(&sa, SCHED_SPORADIC) == 0 && posix_spawnattr_setschedparam(&sa, &p) == 0);
		memset(&q, 0, sizeof q);
		CHECK(posix_spawnattr_getschedparam(&sa, &q) == 0 && !memcmp(&p, &q, sizeof p));
		posix_spawnattr_destroy(&sa);
	}

	/* a thread created as a sporadic server, against a FIFO thread between
	   its priorities */
	pthread_attr_init(&a);
	pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
	CHECK(pthread_attr_setschedpolicy(&a, SCHED_SPORADIC) == 0);
	p = ss(60, 10, 20, 100, 8);
	CHECK(pthread_attr_setschedparam(&a, &p) == 0);
	memset(&q, 0, sizeof q);
	CHECK(pthread_attr_getschedparam(&a, &q) == 0 && !memcmp(&p, &q, sizeof p));
	CHECK(pthread_create(&ts, &a, busy, (void *)0) == 0);
	CHECK(pthread_getschedparam(ts, &pol, &q) == 0 && pol == SCHED_SPORADIC && q.sched_ss_low_priority == 10);
	pthread_attr_setschedpolicy(&a, SCHED_FIFO);
	p.sched_priority = 30;
	CHECK(pthread_attr_setschedparam(&a, &p) == 0);
	CHECK(pthread_create(&tf, &a, busy, (void *)1) == 0);
	{
		struct timespec d = {1, 0};
		nanosleep(&d, 0);
	}
	double cs = cpu(ts), cf = cpu(tf);
	stop = 1;
	pthread_join(ts, 0);
	pthread_join(tf, 0);
	os("server cpu ");
	oi((long long)(cs * 1000));
	os(" ms, fifo ");
	oi((long long)(cf * 1000));
	os(" ms\n");
	/* a fifth of the second, give or take scheduling granularity */
	CHECK(cs > 0.12 && cs < 0.32);
	CHECK(cf > 0.5);

	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}
