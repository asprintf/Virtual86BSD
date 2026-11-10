/* k_timer.c - POSIX timers: overruns (one signal queued per timer at a
 * time; the expirations while it is pending are counted, and
 * timer_getoverrun() and si_overrun give the count when it is taken),
 * also for a signal that was already pending from kill(), and for a
 * blocked standard signal; and (as root) that setting CLOCK_REALTIME
 * back does not lengthen a relative clock_nanosleep on it.  Prints
 * "k_timer ok" or FAIL lines. */
#define _POSIX_C_SOURCE 202405L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static timer_t mk(int sig, long ms)
{
	struct sigevent ev;
	struct itimerspec its;
	timer_t t;
	memset(&ev, 0, sizeof ev);
	ev.sigev_notify = SIGEV_SIGNAL;
	ev.sigev_signo = sig;
	ev.sigev_value.sival_int = 42;
	if (timer_create(CLOCK_MONOTONIC, &ev, &t) != 0) {
		printf("FAIL timer_create: %s\n", strerror(errno));
		failures++;
	}
	its.it_value.tv_sec = ms / 1000;
	its.it_value.tv_nsec = ms % 1000 * 1000000;
	its.it_interval = its.it_value;
	timer_settime(t, 0, &its, NULL);
	return t;
}

static void nap(long ms)
{
	struct timespec ts = { ms / 1000, ms % 1000 * 1000000 };
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

int main(void)
{
	sigset_t set;
	siginfo_t si;
	timer_t t;
	int n, sig;

	/* a real-time signal, blocked: 1 s of a 100 ms timer is one signal
	 * and about 9 overruns */
	sigemptyset(&set);
	sigaddset(&set, SIGRTMIN);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_BLOCK, &set, NULL);
	t = mk(SIGRTMIN, 100);
	nap(1050);
	sig = sigwaitinfo(&set, &si);
	n = timer_getoverrun(t);
	CHECK(sig == SIGRTMIN && si.si_code == SI_TIMER && si.si_value.sival_int == 42);
	CHECK(si.si_overrun == n);
	CHECK(n >= 7 && n <= 10);
	printf("real-time signal: overrun %d (si_overrun %d)\n", n, si.si_overrun);
	/* only one was queued */
	{
		struct timespec z = { 0, 0 };
		timer_delete(t);
		CHECK(sigtimedwait(&set, &si, &z) == -1 && errno == EAGAIN);
	}

	/* a standard signal, blocked, the same */
	t = mk(SIGUSR1, 100);
	nap(1050);
	sig = sigwaitinfo(&set, &si);
	n = timer_getoverrun(t);
	CHECK(sig == SIGUSR1 && si.si_code == SI_TIMER);
	CHECK(n >= 7 && n <= 10);
	printf("standard signal: overrun %d\n", n);
	/* after it was taken, counting starts again */
	nap(350);
	sig = sigwaitinfo(&set, &si);
	n = timer_getoverrun(t);
	CHECK(sig == SIGUSR1 && n >= 1 && n <= 4);
	printf("again: overrun %d\n", n);
	timer_delete(t);

	/* SIGUSR1 pending from kill() first: the timer's expirations are
	 * overruns, and once the signal is taken the timer queues again */
	kill(getpid(), SIGUSR1);
	t = mk(SIGUSR1, 100);
	nap(350);
	sig = sigwaitinfo(&set, &si);
	CHECK(sig == SIGUSR1 && si.si_code == SI_USER);
	nap(150);
	sig = sigwaitinfo(&set, &si);
	CHECK(sig == SIGUSR1 && si.si_code == SI_TIMER);
	timer_delete(t);

	/* a relative sleep of 3 s on CLOCK_REALTIME while the clock is set
	   back 1 s: it still lasts 3 s, so the clock reads 2 s later */
	if (geteuid() == 0) {
		struct timespec t0, t1, d = { 3, 0 };
		int st;
		pid_t p;
		clock_gettime(CLOCK_REALTIME, &t0);
		if ((p = fork()) == 0) {
			clock_nanosleep(CLOCK_REALTIME, 0, &d, NULL);
			clock_gettime(CLOCK_REALTIME, &t1);
			_exit((int)(t1.tv_sec - t0.tv_sec));
		}
		nap(1000);
		CHECK(clock_settime(CLOCK_REALTIME, &t0) == 0);
		waitpid(p, &st, 0);
		clock_gettime(CLOCK_REALTIME, &t1);
		t1.tv_sec += 1;
		clock_settime(CLOCK_REALTIME, &t1);	/* the second taken back */
		CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 2);
		printf("relative sleep across clock_settime: clock +%d s (want 2)\n", WEXITSTATUS(st));
	}

	printf(failures ? "k_timer FAILED\n" : "k_timer ok\n");
	return failures != 0;
}
