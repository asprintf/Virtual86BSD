/* The legacy XSI interfaces (SUSv2-SUSv4): behaviour as the standard's
 * texts say.  Which macros declare them: legacy.sh. */
#define _DEFAULT_SOURCE
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "out.h"

static volatile sig_atomic_t got, masked_in_handler;
static void on_sig(int s)
{
	sigset_t m;
	got++;
	sigprocmask(SIG_BLOCK, 0, &m);
	masked_in_handler = sigismember(&m, s);
}

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static int blocked(int sig)
{
	sigset_t m;
	sigprocmask(SIG_BLOCK, 0, &m);
	return sigismember(&m, sig);
}

static void timer_ms(int ms)	/* SIGALRM once, after ms */
{
	struct itimerval v;
	memset(&v, 0, sizeof v);
	v.it_value.tv_sec = ms / 1000;
	v.it_value.tv_usec = ms % 1000 * 1000;
	setitimer(ITIMER_REAL, &v, 0);
}

int main(void)
{
	struct timeval a, b;
	struct timespec ts;
	struct itimerval v, o;
	long long t0, dt;
	void (*old)(int);
	sigset_t pend;
	struct sigaction sa;
	int fds[2], st, r;
	pid_t pid;
	char c;

	/* gettimeofday */
	CHECK(gettimeofday(&a, 0) == 0);
	clock_gettime(CLOCK_REALTIME, &ts);
	CHECK(a.tv_usec >= 0 && a.tv_usec < 1000000);
	CHECK(a.tv_sec >= ts.tv_sec - 1 && a.tv_sec <= ts.tv_sec + 1);
	CHECK(gettimeofday(&a, (void *)&st) == 0);	/* tzp: left alone */
	usleep(30000);
	CHECK(gettimeofday(&b, 0) == 0);
	dt = (b.tv_sec - a.tv_sec) * 1000000LL + (b.tv_usec - a.tv_usec);
	CHECK(dt >= 25000 && dt < 1000000);

	/* usleep */
	t0 = now_ms();
	CHECK(usleep(0) == 0);
	CHECK(now_ms() - t0 < 20);
	t0 = now_ms();
	CHECK(usleep(60000) == 0);
	dt = now_ms() - t0;
	CHECK(dt >= 55 && dt < 500);
	/* one million or more: "may fail" with EINVAL; this one sleeps */
	t0 = now_ms();
	CHECK(usleep(1000000) == 0);
	dt = now_ms() - t0;
	CHECK(dt >= 990 && dt < 2500);
	/* a signal that is caught ends it: -1, EINTR */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGALRM, &sa, 0);
	got = 0;
	timer_ms(30);
	t0 = now_ms();
	errno = 0;
	CHECK(usleep(800000) == -1 && errno == EINTR);
	CHECK(got == 1 && now_ms() - t0 < 600);

	/* getitimer, setitimer */
	memset(&v, 0, sizeof v);
	v.it_value.tv_sec = 5;
	v.it_interval.tv_usec = 250000;
	CHECK(setitimer(ITIMER_REAL, &v, 0) == 0);
	CHECK(getitimer(ITIMER_REAL, &o) == 0);
	CHECK(o.it_value.tv_sec >= 4 && o.it_value.tv_sec <= 5);
	CHECK(o.it_interval.tv_sec == 0 && o.it_interval.tv_usec == 250000);
	memset(&v, 0, sizeof v);
	CHECK(setitimer(ITIMER_REAL, &v, &o) == 0);	/* stops it, gives the old */
	CHECK(o.it_value.tv_sec >= 4 && o.it_interval.tv_usec == 250000);
	CHECK(getitimer(ITIMER_REAL, &o) == 0);
	CHECK(o.it_value.tv_sec == 0 && o.it_value.tv_usec == 0);
	got = 0;
	memset(&v, 0, sizeof v);
	v.it_value.tv_usec = 20000;
	v.it_interval.tv_usec = 20000;
	CHECK(setitimer(ITIMER_REAL, &v, 0) == 0);
	t0 = now_ms();
	while (got < 3 && now_ms() - t0 < 2000)
		usleep(5000);
	CHECK(got >= 3);			/* an interval repeats */
	memset(&v, 0, sizeof v);
	setitimer(ITIMER_REAL, &v, 0);
	errno = 0;
	CHECK(getitimer(99, &o) == -1 && errno == EINVAL);
	errno = 0;
	CHECK(setitimer(99, &v, 0) == -1 && errno == EINVAL);
	v.it_value.tv_usec = 1000000;
	errno = 0;
	CHECK(setitimer(ITIMER_REAL, &v, 0) == -1 && errno == EINVAL);

	/* sighold, sigrelse: blocked signals stay pending until released */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGUSR1, &sa, 0);
	got = 0;
	CHECK(sighold(SIGUSR1) == 0 && blocked(SIGUSR1));
	kill(getpid(), SIGUSR1);
	CHECK(got == 0);
	sigpending(&pend);
	CHECK(sigismember(&pend, SIGUSR1) == 1);
	CHECK(sigrelse(SIGUSR1) == 0 && !blocked(SIGUSR1));
	CHECK(got == 1);
	errno = 0; CHECK(sighold(0) == -1 && errno == EINVAL);
	errno = 0; CHECK(sighold(999) == -1 && errno == EINVAL);
	errno = 0; CHECK(sigrelse(-1) == -1 && errno == EINVAL);

	/* sigignore */
	CHECK(sigignore(SIGUSR2) == 0);
	CHECK(sigaction(SIGUSR2, 0, &sa) == 0 && sa.sa_handler == SIG_IGN);
	kill(getpid(), SIGUSR2);		/* nothing happens */
	errno = 0; CHECK(sigignore(SIGKILL) == -1 && errno == EINVAL);
	errno = 0; CHECK(sigignore(SIGSTOP) == -1 && errno == EINVAL);
	errno = 0; CHECK(sigignore(999) == -1 && errno == EINVAL);

	/* sigset: returns the old disposition, or SIG_HOLD if it was blocked */
	CHECK(sigset(SIGUSR2, on_sig) == SIG_IGN);
	CHECK(sigset(SIGUSR2, SIG_DFL) == on_sig);
	CHECK(sigset(SIGUSR2, SIG_DFL) == SIG_DFL);
	CHECK(sigset(SIGUSR2, SIG_IGN) == SIG_DFL);
	CHECK(sigset(SIGUSR2, SIG_DFL) == SIG_IGN);
	got = 0;
	CHECK(sigset(SIGUSR1, on_sig) == on_sig);	/* it was on_sig above */
	CHECK(sigset(SIGUSR1, SIG_HOLD) == on_sig);	/* blocks; keeps the handler */
	CHECK(blocked(SIGUSR1));
	CHECK(sigaction(SIGUSR1, 0, &sa) == 0 && sa.sa_handler == on_sig);
	kill(getpid(), SIGUSR1);
	CHECK(got == 0);
	CHECK(sigset(SIGUSR1, on_sig) == SIG_HOLD);	/* was blocked; unblocks */
	CHECK(!blocked(SIGUSR1));
	CHECK(got == 1);			/* the pending one arrives */
	CHECK(masked_in_handler == 1);		/* handler runs with sig blocked */
	CHECK(!blocked(SIGUSR1));		/* and the mask is restored after */
	errno = 0; CHECK(sigset(SIGKILL, on_sig) == SIG_ERR && errno == EINVAL);
	errno = 0; CHECK(sigset(SIGSTOP, SIG_IGN) == SIG_ERR && errno == EINVAL);
	errno = 0; CHECK(sigset(999, on_sig) == SIG_ERR && errno == EINVAL);
	errno = 0; CHECK(sigset(0, SIG_HOLD) == SIG_ERR && errno == EINVAL);
	sigset(SIGUSR1, SIG_DFL);
	sigset(SIGUSR2, SIG_DFL);

	/* sigpause: removes sig from the mask, waits, restores the mask, -1 EINTR */
	got = 0;
	sigset(SIGALRM, on_sig);
	CHECK(sighold(SIGALRM) == 0);
	timer_ms(30);
	errno = 0;
	r = sigpause(SIGALRM);
	CHECK(r == -1 && errno == EINTR);
	CHECK(got == 1);
	CHECK(blocked(SIGALRM));		/* as it was before */
	sigrelse(SIGALRM);
	errno = 0; CHECK(sigpause(999) == -1 && errno == EINVAL);

	/* siginterrupt: the flag is SA_RESTART turned off (1) or on (0) */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGALRM, &sa, 0);
	CHECK(siginterrupt(SIGALRM, 0) == 0);
	CHECK(sigaction(SIGALRM, 0, &sa) == 0 && (sa.sa_flags & SA_RESTART));
	CHECK(sa.sa_handler == on_sig);
	CHECK(siginterrupt(SIGALRM, 1) == 0);
	CHECK(sigaction(SIGALRM, 0, &sa) == 0 && !(sa.sa_flags & SA_RESTART));
	errno = 0; CHECK(siginterrupt(999, 1) == -1 && errno == EINVAL);
	/* ... and what it does to a read that a signal interrupts */
	for (r = 0; r < 2; r++) {		/* 1: interrupted, 0: restarted */
		int flag = r == 0 ? 1 : 0;
		CHECK(pipe(fds) == 0);
		siginterrupt(SIGALRM, flag);
		pid = fork();
		if (pid == 0) {
			close(fds[0]);
			usleep(250000);
			write(fds[1], "x", 1);
			_exit(0);
		}
		close(fds[1]);
		got = 0;
		timer_ms(40);
		errno = 0;
		c = 0;
		st = (int)read(fds[0], &c, 1);
		if (flag) CHECK(st == -1 && errno == EINTR);
		else CHECK(st == 1 && c == 'x');
		CHECK(got == 1);
		close(fds[0]);
		waitpid(pid, &st, 0);
	}

	/* setpgrp: in a child that is not a session leader */
	pid = fork();
	if (pid == 0) {
		pid_t g = setpgrp();
		_exit(g == getpid() && getpgrp() == getpid() ? 0 : 1);
	}
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	/* a session leader: no effect, no error, the group is returned */
	pid = fork();
	if (pid == 0) {
		pid_t g;
		setsid();
		g = setpgrp();
		_exit(g == getpid() && getpgrp() == g && getsid(0) == getpid() ? 0 : 1);
	}
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);

	os(failures ? "legacy FAILED\n" : "legacy ok\n");
	oflush();
	return failures != 0;
}
