/* ITIMER_VIRTUAL and ITIMER_PROF count the process's CPU time, and send
 * SIGVTALRM and SIGPROF when it is used up (they were accepted and never
 * expired); ITIMER_REAL, as before, counts time that passes. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static volatile sig_atomic_t vt, pf, al;
static void on_vt(int s) { (void)s; vt++; }
static void on_pf(int s) { (void)s; pf++; }
static void on_al(int s) { (void)s; al++; }

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void nap(int ms)
{
	struct timespec ts = { ms / 1000, ms % 1000 * 1000000L };
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
		;
}

/* uses CPU until *c reaches n, or give up after limit ms (returns the ms it took) */
static long long spin(volatile sig_atomic_t *c, int n, int limit)
{
	long long t0 = now_ms();
	while (*c < n && now_ms() - t0 < limit)
		;
	return now_ms() - t0;
}

static void set(int which, long first_ms, long interval_ms)
{
	struct itimerval v;
	memset(&v, 0, sizeof v);
	v.it_value.tv_sec = first_ms / 1000;
	v.it_value.tv_usec = first_ms % 1000 * 1000;
	v.it_interval.tv_sec = interval_ms / 1000;
	v.it_interval.tv_usec = interval_ms % 1000 * 1000;
	CHECK(setitimer(which, &v, 0) == 0);
}

static void cpu_timer(int which, int sig, void (*h)(int), volatile sig_atomic_t *c, const char *name)
{
	struct itimerval v;
	long long dt;
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = h;
	CHECK(sigaction(sig, &sa, 0) == 0);

	*c = 0;
	set(which, 30, 0);
	CHECK(getitimer(which, &v) == 0);
	/* (a tick more is allowed: Linux counts the one running) */
	CHECK(v.it_value.tv_sec == 0 && v.it_value.tv_usec > 0 && v.it_value.tv_usec <= 40000);
	dt = spin(c, 1, 5000);
	printf("%s: one-shot of 30 ms of CPU time came after %lld ms\n", name, dt);
	CHECK(*c == 1);
	CHECK(dt >= 25);			/* CPU time cannot pass faster than the clock */
	CHECK(getitimer(which, &v) == 0 && v.it_value.tv_sec == 0 && v.it_value.tv_usec == 0);
	spin(c, 2, 100);
	CHECK(*c == 1);				/* one-shot: once */

	*c = 0;
	set(which, 20, 20);
	dt = spin(c, 3, 5000);
	printf("%s: interval of 20 ms of CPU time gave 3 signals after %lld ms\n", name, dt);
	CHECK(*c >= 3);
	set(which, 0, 0);			/* off */
	CHECK(getitimer(which, &v) == 0 && v.it_value.tv_usec == 0 && v.it_interval.tv_usec == 0);
	*c = 0;
	spin(c, 1, 100);
	CHECK(*c == 0);

	/* sleeping uses no CPU time: the timer does not expire */
	set(which, 200, 0);
	nap(500);
	CHECK(*c == 0);
	set(which, 0, 0);
}

int main(void)
{
	struct sigaction sa;
	struct itimerval v;

	cpu_timer(ITIMER_VIRTUAL, SIGVTALRM, on_vt, &vt, "ITIMER_VIRTUAL");
	cpu_timer(ITIMER_PROF, SIGPROF, on_pf, &pf, "ITIMER_PROF");

	/* ITIMER_REAL counts the time that passes, asleep too */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_al;
	sigaction(SIGALRM, &sa, 0);
	al = 0;
	set(ITIMER_REAL, 30, 0);
	nap(200);
	CHECK(al == 1);

	/* a number of microseconds out of range, and a timer that does not exist */
	memset(&v, 0, sizeof v);
	v.it_value.tv_usec = 1000000;
	errno = 0;
	CHECK(setitimer(ITIMER_VIRTUAL, &v, 0) == -1 && errno == EINVAL);
	errno = 0;
	CHECK(setitimer(3, &v, 0) == -1 && errno == EINVAL);

	printf(failures ? "k_itimer FAILED\n" : "k_itimer ok\n");
	return failures != 0;
}
