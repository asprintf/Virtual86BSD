/* User tasks that run on several processors at the same time: counters kept by
 * mutexes (plain, priority inheriting, robust), a fork while sibling threads
 * write, thread join storms, fork/exit/wait storms, signals to a thread that
 * runs on another processor, an interval timer while threads spin, a ring of
 * pipes, exec with threads spinning, with threads making threads, and with a
 * thread in vfork, the monotonic clock seen from two processors.
 * It also passes with one processor. */
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

#define NT 4
#define PG 4096

static double now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static void nap(long us)
{
	struct timespec ts = {us / 1000000, us % 1000000 * 1000L};

	nanosleep(&ts, NULL);
}

/* ---- counters kept by a mutex ---- */
static pthread_mutex_t mu;
static long counter;
#define ITERS 20000

static void *count_worker(void *arg)
{
	(void)arg;
	for (int i = 0; i < ITERS; i++) {
		int e = pthread_mutex_lock(&mu);
		if (e == EOWNERDEAD)
			pthread_mutex_consistent(&mu);
		counter++;
		pthread_mutex_unlock(&mu);
	}
	return NULL;
}

static void mutex_counter(int protocol, int robust)
{
	pthread_mutexattr_t a;
	pthread_t t[NT];

	pthread_mutexattr_init(&a);
	if (protocol)
		pthread_mutexattr_setprotocol(&a, protocol);
	if (robust)
		pthread_mutexattr_setrobust(&a, PTHREAD_MUTEX_ROBUST);
	CHECK(pthread_mutex_init(&mu, &a) == 0);
	counter = 0;
	for (int i = 0; i < NT; i++)
		CHECK(pthread_create(&t[i], NULL, count_worker, NULL) == 0);
	for (int i = 0; i < NT; i++)
		pthread_join(t[i], NULL);
	CHECK(counter == (long)NT * ITERS);
	pthread_mutex_destroy(&mu);
	pthread_mutexattr_destroy(&a);
}

/* a thread that dies holding a robust mutex, while the others use it */
static void *die_locked(void *arg)
{
	pthread_mutex_lock(arg);
	return NULL;
}

static void robust_owner_death(void)
{
	pthread_mutexattr_t a;
	pthread_mutex_t m;
	int dead = 0;

	pthread_mutexattr_init(&a);
	pthread_mutexattr_setrobust(&a, PTHREAD_MUTEX_ROBUST);
	pthread_mutex_init(&m, &a);
	for (int i = 0; i < 300; i++) {
		pthread_t t;
		int e;

		CHECK(pthread_create(&t, NULL, die_locked, &m) == 0);
		pthread_join(t, NULL);
		e = pthread_mutex_lock(&m);
		if (e == EOWNERDEAD) {
			dead++;
			pthread_mutex_consistent(&m);
		} else
			CHECK(e == 0);
		pthread_mutex_unlock(&m);
	}
	CHECK(dead == 300);
	pthread_mutex_destroy(&m);
}

/* ---- fork while sibling threads write ---- */
#define NW 3
static char *mem;
static volatile int stop;

static void *writer(void *arg)
{
	volatile unsigned long *w = (volatile unsigned long *)(mem + (long)arg * PG);

	while (!stop)
		for (int i = 0; i < 64; i++)
			w[i] = w[i] + 1;
	return NULL;
}

static void fork_write(void)
{
	pthread_t t[NW];
	int bad = 0;

	mem = mmap(NULL, (NW + 1) * PG, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(mem != MAP_FAILED);
	stop = 0;
	for (long i = 0; i < NW; i++)
		CHECK(pthread_create(&t[i], NULL, writer, (void *)i) == 0);
	for (int r = 0; r < 100; r++) {
		unsigned long before[NW];
		pid_t p;
		int st;

		for (int i = 0; i < NW; i++)
			before[i] = ((volatile unsigned long *)(mem + i * PG))[0];
		p = fork();
		if (p == 0) {
			/* the child's copy must not change under it, and its own writes stay its own */
			unsigned long s1[NW * 64], s2[NW * 64];
			volatile unsigned long *m = (volatile unsigned long *)mem;
			double t0 = now();

			for (int i = 0; i < NW; i++)
				for (int j = 0; j < 64; j++)
					s1[i * 64 + j] = m[i * (PG / 8) + j];
			while (now() - t0 < 0.002)
				;
			for (int i = 0; i < NW; i++)
				for (int j = 0; j < 64; j++)
					s2[i * 64 + j] = m[i * (PG / 8) + j];
			_exit(memcmp(s1, s2, sizeof s1) ? 1 : 0);
		}
		CHECK(p > 0);
		CHECK(waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0))
			bad++;
		for (int i = 0; i < NW; i++)
			CHECK(((volatile unsigned long *)(mem + i * PG))[0] >= before[i]);
		if (bad > 3)
			break;
	}
	stop = 1;
	for (int i = 0; i < NW; i++)
		pthread_join(t[i], NULL);
	munmap(mem, (NW + 1) * PG);
}

/* the child writes its copy while the parent's threads write theirs: no write crosses */
static void cow_apart(void)
{
	int bad = 0;

	mem = mmap(NULL, (NW + 1) * PG, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(mem != MAP_FAILED);
	memset(mem + NW * PG, 0x11, PG);
	for (int r = 0; r < 100; r++) {
		pid_t p;
		int st;
		pthread_t t[NW];

		stop = 0;
		for (long i = 0; i < NW; i++)
			CHECK(pthread_create(&t[i], NULL, writer, (void *)i) == 0);
		p = fork();
		if (p == 0) {
			volatile unsigned char *q = (volatile unsigned char *)mem + NW * PG;
			int ok = 1;

			for (int k = 0; k < 50 && ok; k++) {
				for (int i = 0; i < PG; i++)
					q[i] = (unsigned char)(0x20 + k);
				for (int i = 0; i < PG; i++)
					if (q[i] != (unsigned char)(0x20 + k))
						ok = 0;
			}
			_exit(ok ? 0 : 1);
		}
		/* the parent's page next to the child's must keep its pattern */
		for (int k = 0; k < 20; k++) {
			volatile unsigned char *q = (volatile unsigned char *)mem + NW * PG;
			for (int i = 0; i < PG; i++)
				if (q[i] != 0x11)
					bad++;
		}
		CHECK(waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		stop = 1;
		for (int i = 0; i < NW; i++)
			pthread_join(t[i], NULL);
	}
	CHECK(bad == 0);
	munmap(mem, (NW + 1) * PG);
}

/* ---- thread create/join storm ---- */
static pthread_mutex_t jm = PTHREAD_MUTEX_INITIALIZER;
static long joined;

static void *join_worker(void *arg)
{
	(void)arg;
	pthread_mutex_lock(&jm);
	joined++;
	pthread_mutex_unlock(&jm);
	return NULL;
}

static void join_storm(void)
{
	joined = 0;
	for (int r = 0; r < 1000; r++) {
		pthread_t t[NT];

		for (int i = 0; i < NT; i++)
			CHECK(pthread_create(&t[i], NULL, join_worker, NULL) == 0);
		for (int i = 0; i < NT; i++)
			pthread_join(t[i], NULL);
	}
	CHECK(joined == 1000L * NT);
}

/* ---- fork, exit, wait ---- */
static void fork_storm(void)
{
	for (int r = 0; r < 60; r++) {
		pid_t p[8];
		int st;

		for (int i = 0; i < 8; i++) {
			p[i] = fork();
			if (p[i] == 0)
				_exit(i + 1);
			CHECK(p[i] > 0);
		}
		for (int i = 0; i < 8; i++) {
			CHECK(waitpid(p[i], &st, 0) == p[i] && WIFEXITED(st) && WEXITSTATUS(st) == i + 1);
		}
	}
}

/* ---- signals to a thread that runs on another processor ---- */
static volatile int caught, spin_stop;

static void on_usr1(int sig)
{
	(void)sig;
	caught++;
}

static void *spinner(void *arg)
{
	(void)arg;
	while (!spin_stop)
		;
	return NULL;
}

static void signal_running(void)
{
	struct sigaction sa;
	pthread_t t;
	int lost = 0;
	double worst = 0;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_usr1;
	sigemptyset(&sa.sa_mask);
	CHECK(sigaction(SIGUSR1, &sa, NULL) == 0);
	caught = 0;
	spin_stop = 0;
	CHECK(pthread_create(&t, NULL, spinner, NULL) == 0);
	for (int i = 0; i < 300; i++) {
		double t0 = now();

		pthread_kill(t, SIGUSR1);
		while (caught <= i && now() - t0 < 1.0)
			nap(100);
		if (caught <= i)
			lost++;
		if (now() - t0 > worst)
			worst = now() - t0;
	}
	spin_stop = 1;
	pthread_join(t, NULL);
	printf("signal to a running thread: worst %d us\n", (int)(worst * 1e6));
	CHECK(lost == 0);
	CHECK(caught == 300);
}

/* ---- an interval timer at 1 kHz while threads spin ---- */
static volatile int ticks;

static void on_alrm(int sig)
{
	(void)sig;
	ticks++;
}

static void itimer_storm(void)
{
	struct sigaction sa;
	struct itimerval it = {{0, 1000}, {0, 1000}}, off = {{0, 0}, {0, 0}};
	pthread_t t[3];
	double t0;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_alrm;
	sigemptyset(&sa.sa_mask);
	CHECK(sigaction(SIGALRM, &sa, NULL) == 0);
	ticks = 0;
	spin_stop = 0;
	for (int i = 0; i < 3; i++)
		CHECK(pthread_create(&t[i], NULL, spinner, NULL) == 0);
	CHECK(setitimer(ITIMER_REAL, &it, NULL) == 0);
	for (t0 = now(); now() - t0 < 0.5;)
		nap(1000);
	setitimer(ITIMER_REAL, &off, NULL);
	spin_stop = 1;
	for (int i = 0; i < 3; i++)
		pthread_join(t[i], NULL);
	printf("interval timer: %d signals in half a second\n", ticks);
	CHECK(ticks > 100);
}

/* ---- a ring of processes passing a token through pipes ---- */
static void pipe_ring(void)
{
	enum { N = 4, LAPS = 2000 };
	int fd[N][2];
	pid_t p[N];
	char c = 'x';
	int st;

	for (int i = 0; i < N; i++)
		CHECK(pipe(fd[i]) == 0);
	for (int i = 1; i < N; i++) {
		p[i] = fork();
		if (p[i] == 0) {
			/* only its own ends: else nobody sees the end of file */
			for (int j = 0; j < N; j++) {
				if (j != i - 1)
					close(fd[j][0]);
				if (j != i)
					close(fd[j][1]);
			}
			for (;;) {
				if (read(fd[i - 1][0], &c, 1) != 1)
					_exit(0);
				if (write(fd[i][1], &c, 1) != 1)
					_exit(1);
			}
		}
		CHECK(p[i] > 0);
	}
	for (int k = 0; k < LAPS; k++) {
		CHECK(write(fd[0][1], &c, 1) == 1);
		CHECK(read(fd[N - 1][0], &c, 1) == 1);
	}
	for (int i = 0; i < N; i++) {
		close(fd[i][0]);
		close(fd[i][1]);
	}
	for (int i = 1; i < N; i++) {
		CHECK(waitpid(p[i], &st, 0) == p[i]);
		CHECK(WIFEXITED(st));
	}
}

/* ---- exec with threads spinning ---- */
static void *spin_forever(void *arg)
{
	(void)arg;
	for (;;)
		;
	return NULL;
}

static void exec_spinning(void)
{
	for (int r = 0; r < 30; r++) {
		pid_t p = fork();
		int st;

		if (p == 0) {
			pthread_t t;

			for (int i = 0; i < 3; i++)
				pthread_create(&t, NULL, spin_forever, NULL);
			nap(2000);
			execl("/usr/bin/true", "true", (char *)NULL);
			_exit(127);
		}
		CHECK(p > 0);
		CHECK(waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
}

/* ---- system calls from several threads at once ---- */
static void *getpid_worker(void *arg)
{
	pid_t me = getpid();

	for (int i = 0; i < 50000; i++)
		if (getpid() != me)
			return arg;
	return NULL;
}

static void getpid_loops(void)
{
	pthread_t t[NT];
	void *r;

	for (int i = 0; i < NT; i++)
		CHECK(pthread_create(&t[i], NULL, getpid_worker, NULL) == 0);
	for (int i = 0; i < NT; i++) {
		pthread_join(t[i], &r);
		CHECK(r == NULL);
	}
}

/* a process that does not end within secs is killed */
static int wait_for(pid_t p, int secs, int *st)
{
	for (int i = 0; i < secs * 1000; i++) {
		if (waitpid(p, st, WNOHANG) == p)
			return 1;
		nap(1000);
	}
	kill(p, SIGKILL);
	waitpid(p, st, 0);
	return 0;
}

/* threads that make threads, which execute at once, while the main thread executes */
static void *exec_now(void *arg)
{
	(void)arg;
	execl("/usr/bin/true", "true", (char *)NULL);
	return NULL;
}

static void *thread_maker(void *arg)
{
	(void)arg;
	for (;;) {
		pthread_t t;

		if (pthread_create(&t, NULL, exec_now, NULL) == 0)
			pthread_detach(t);
		else
			nap(100);
	}
	return NULL;
}

static void exec_race(void)
{
	for (int r = 0; r < 20; r++) {
		pid_t p = fork();
		int st = 0;

		if (p == 0) {
			pthread_t t;

			pthread_create(&t, NULL, thread_maker, NULL);
			nap(2000);
			execl("/usr/bin/true", "true", (char *)NULL);
			_exit(127);
		}
		CHECK(p > 0);
		CHECK(wait_for(p, 10, &st));
		CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
}

/* a thread in vfork, whose child waits: exec does not wait for the child, kill -9 ends the process */
pid_t vfork(void); /* (SUSv3: not declared at 700) */

static void *vforker(void *arg)
{
	(void)arg;
	if (vfork() == 0) {
		alarm(5);
		pause();
		_exit(0);
	}
	return NULL;
}

static void exec_vfork(void)
{
	pid_t p = fork();
	int st = 0;

	if (p == 0) {
		pthread_t t;

		pthread_create(&t, NULL, vforker, NULL);
		nap(20000);
		execl("/usr/bin/true", "true", (char *)NULL);
		_exit(127);
	}
	CHECK(p > 0);
	CHECK(wait_for(p, 3, &st));
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static void kill_vfork(void)
{
	pid_t p = fork();
	int st = 0;

	if (p == 0) {
		pthread_t t;

		pthread_create(&t, NULL, vforker, NULL);
		for (;;)
			nap(100000);
	}
	CHECK(p > 0);
	nap(50000);
	kill(p, SIGKILL);
	CHECK(wait_for(p, 3, &st));
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
}

/* ---- the monotonic clock, read by two threads in turn, never goes back ---- */
static volatile int turn, clock_bad;
static volatile double clock_last;

static void *clock_peer(void *arg)
{
	int me = (int)(long)arg;

	for (int i = 0; i < 5000; i++) {
		double t;

		while (turn != me)
			sched_yield();
		t = now();
		if (t < clock_last)
			clock_bad++;
		clock_last = t;
		turn = 1 - me;
	}
	return NULL;
}

static void clock_order(void)
{
	pthread_t t[2];

	turn = 0;
	clock_bad = 0;
	clock_last = 0;
	for (long i = 0; i < 2; i++)
		CHECK(pthread_create(&t[i], NULL, clock_peer, (void *)i) == 0);
	for (int i = 0; i < 2; i++)
		pthread_join(t[i], NULL);
	CHECK(clock_bad == 0);
}

#define RUN(f) do { printf("%s\n", #f); f; } while (0)

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	RUN(mutex_counter(0, 0));
	RUN(mutex_counter(PTHREAD_PRIO_INHERIT, 0));
	RUN(mutex_counter(0, 1));
	RUN(robust_owner_death());
	RUN(fork_write());
	RUN(cow_apart());
	RUN(join_storm());
	RUN(fork_storm());
	RUN(signal_running());
	RUN(itimer_storm());
	RUN(pipe_ring());
	RUN(exec_spinning());
	RUN(exec_race());
	RUN(exec_vfork());
	RUN(kill_vfork());
	RUN(clock_order());
	RUN(getpid_loops());
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures != 0;
}
