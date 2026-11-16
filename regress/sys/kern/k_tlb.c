/* A mapping that mprotect or munmap changed is changed for every thread when
 * the call returns, on whatever processor the thread runs: a thread that
 * writes to a page must not get a write in after mprotect(PROT_READ) has
 * returned, and one that reads a page must fault after munmap. */
#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PG 4096
#define ROUNDS 3000

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static volatile unsigned *page;
static volatile int stop, mode; /* what the worker does: 0 nothing, 1 writes the page, 2 reads it */
static volatile unsigned long faults, steps;
static sigjmp_buf jb;

static void nap(long us)
{
	struct timespec ts = {0, us * 1000L};

	nanosleep(&ts, NULL);
}

static void on_segv(int sig)
{
	(void)sig;
	faults++;
	siglongjmp(jb, 1);
}

static void *worker(void *arg)
{
	(void)arg;
	if (sigsetjmp(jb, 1))
		nap(100); /* it faulted: not at once again */
	while (!stop) {
		if (mode == 1)
			*page += 1;
		else if (mode == 2)
			(void)*page;
		else
			nap(20);
		steps++;
	}
	return NULL;
}

/* until the worker has done something more */
static void worker_runs(void)
{
	unsigned long s = steps;

	for (int i = 0; i < 20000 && steps == s; i++)
		nap(10);
}

int main(void)
{
	pthread_t t;
	struct sigaction sa;
	unsigned v1, v2;
	char *a;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_segv;
	sigemptyset(&sa.sa_mask);
	CHECK(sigaction(SIGSEGV, &sa, NULL) == 0);
	a = mmap(NULL, PG, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(a != MAP_FAILED);
	page = (volatile unsigned *)a;
	CHECK(pthread_create(&t, NULL, worker, NULL) == 0);

	/* mprotect: the writes stop when the call returns */
	for (int i = 0; i < ROUNDS; i++) {
		mode = 1;
		worker_runs();
		CHECK(mprotect(a, PG, PROT_READ) == 0);
		v1 = *page;
		nap(150);
		v2 = *page;
		if (v1 != v2) {
			printf("FAIL round %d: the page changed after mprotect returned (%u, %u)\n", i, v1, v2);
			failures++;
			break;
		}
		CHECK(mprotect(a, PG, PROT_READ | PROT_WRITE) == 0);
	}

	/* munmap: the reads fault when the call returns */
	for (int i = 0; i < ROUNDS / 3; i++) {
		unsigned long f;

		mode = 2;
		worker_runs();
		f = faults;
		CHECK(munmap(a, PG) == 0);
		for (int k = 0; k < 20000 && faults == f; k++)
			nap(10);
		if (faults == f) {
			printf("FAIL round %d: the page was still read after munmap returned\n", i);
			failures++;
			break;
		}
		mode = 0;
		worker_runs();
		CHECK(mmap(a, PG, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == (void *)a);
	}
	stop = 1;
	pthread_join(t, NULL);
	if (failures == 0)
		printf("k_tlb: ok\n");
	return failures != 0;
}
