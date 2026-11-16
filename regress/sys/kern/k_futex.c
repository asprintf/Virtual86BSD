/* Futex words: FUTEX_WAKE_OP, the priority-inheritance protocol, the faults,
 * and the words the kernel writes when a thread exits (robust list,
 * clear_child_tid). */
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef FUTEX_NR
#define FUTEX_NR 202 /* Linux's; the Makefile gives the system's */
#define ROBUST_NR 273
#define TIDADDR_NR 218
#endif
long rawsys6(long nr, long a, long b, long c, long d, long e, long f);

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); failures++; } } while (0)

#define FUTEX_WAIT 0
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_PRIVATE 128

#define OP_SET 0
#define OP_ADD 1
#define OP_OR 2
#define OP_ANDN 3
#define OP_XOR 4
#define CMP_EQ 0
#define CMP_NE 1
#define FOP(op, arg, cmp, carg) ((unsigned)(op) << 28 | (unsigned)(cmp) << 24 | (unsigned)(arg) << 12 | (carg))

#define WAITERS 0x80000000U
#define OWNER_DIED 0x40000000U

typedef volatile unsigned word;

static long fx(word *a, long op, long val, const void *ts, word *a2, long val3)
{
	return rawsys6(FUTEX_NR, (long)a, op, val, (long)ts, (long)a2, val3);
}

static void nap(long ms)
{
	struct timespec ts = {0, ms * 1000000L};

	nanosleep(&ts, NULL);
}

/* an absolute CLOCK_REALTIME deadline for LOCK_PI, secs from now */
static struct timespec deadline(int secs)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += secs;
	return ts;
}

static void ops(void)
{
	word w[2] = {0, 5};
	long po = FUTEX_PRIVATE | FUTEX_WAKE_OP;

	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_SET, 7, CMP_EQ, 0)) == 0 && w[1] == 7);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_ADD, 3, CMP_EQ, 0)) == 0 && w[1] == 10);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_OR, 0x30, CMP_EQ, 0)) == 0 && w[1] == 0x3a);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_ANDN, 0x0a, CMP_EQ, 0)) == 0 && w[1] == 0x30);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_XOR, 0xff, CMP_EQ, 0)) == 0 && w[1] == 0xcf);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], FOP(OP_SET, 4, CMP_EQ, 0) | 0x80000000U) == 0 && w[1] == 16);
	CHECK(fx(&w[0], po, 0, (void *)0, &w[1], 5U << 28) == -ENOSYS && w[1] == 16);
}

/* the second word is changed whatever the comparison says; its waiter is
   woken only if the comparison with the old value holds */
static void wake_op_cmp(word *s)
{
	int p[2], st, i;
	long r;
	char c;
	pid_t k;
	struct timespec to = {20, 0};

	s[0] = s[1] = 0;
	CHECK(pipe(p) == 0);
	k = fork();
	if (k == 0) {
		(void)!write(p[1], "x", 1);
		_exit(fx(&s[1], FUTEX_WAIT, 0, &to, 0, 0) == 0 ? 0 : 1);
	}
	CHECK(k > 0 && read(p[0], &c, 1) == 1);
	nap(50);
	r = fx(&s[0], FUTEX_WAKE_OP, 1, (void *)1, &s[1], FOP(OP_OR, 0, CMP_NE, 0));
	CHECK(r == 0);
	for (r = 0, i = 0; i < 1000 && r != 1; i++, nap(10))
		r = fx(&s[0], FUTEX_WAKE_OP, 1, (void *)1, &s[1], FOP(OP_OR, 0, CMP_EQ, 0));
	CHECK(r == 1);
	CHECK(waitpid(k, &st, 0) == k && WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static void pi(word *s)
{
	int p[2], st;
	pid_t k;
	char c;
	struct timespec dl;
	unsigned me = (unsigned)getpid();

	s[0] = 0;
	CHECK(fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == 0 && s[0] == me);
	CHECK(fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == -EDEADLK);
	CHECK(fx(&s[0], FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0) == 0 && s[0] == 0);
	CHECK(fx(&s[0], FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0) == -EPERM);
	/* an owner that is not there: taken, and told so; the waiters bit stays */
	s[0] = 0x3ffffff0U;
	CHECK(fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == 0 && s[0] == (me | OWNER_DIED));
	s[0] = 0x3ffffff0U | WAITERS;
	CHECK(fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == 0 && s[0] == (me | OWNER_DIED | WAITERS));
	/* an owner that is there */
	s[0] = 1; /* init */
	CHECK(fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == -EBUSY);
	/* a contended lock: the owner's unlock hands it over */
	s[0] = 0;
	CHECK(pipe(p) == 0);
	k = fork();
	if (k == 0) {
		long r = fx(&s[0], FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0);

		(void)!write(p[1], "x", 1);
		nap(100);
		r |= fx(&s[0], FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0);
		_exit(r == 0 ? 0 : 1);
	}
	CHECK(k > 0 && read(p[0], &c, 1) == 1);
	dl = deadline(20);
	CHECK(fx(&s[0], FUTEX_LOCK_PI, 0, &dl, 0, 0) == 0);
	CHECK(s[0] == me);
	CHECK(waitpid(k, &st, 0) == k && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(fx(&s[0], FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0) == 0 && s[0] == 0);
}

static void faults(void)
{
	char *m = mmap(0, 3 * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	word *ro, *hole, w[2] = {0, 0};
	long pf = FUTEX_PRIVATE;

	CHECK(m != MAP_FAILED);
	ro = (word *)m;
	hole = (word *)(m + 4096);
	*ro = 0;
	CHECK(mprotect(m, 4096, PROT_READ) == 0);
	CHECK(munmap(m + 4096, 4096) == 0);
	CHECK(fx(ro, pf | FUTEX_WAIT, 1, (void *)0, 0, 0) == -EAGAIN);
	CHECK(fx(&w[0], pf | FUTEX_WAKE_OP, 0, (void *)0, ro, FOP(OP_SET, 1, CMP_EQ, 0)) == -EFAULT && *ro == 0);
	CHECK(fx(ro, pf | FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == -EFAULT);
	CHECK(fx(ro, pf | FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0) == -EPERM);
	CHECK(fx(hole, pf | FUTEX_WAIT, 0, (void *)0, 0, 0) == -EFAULT);
	CHECK(fx(&w[0], pf | FUTEX_WAKE_OP, 0, (void *)0, hole, FOP(OP_ADD, 1, CMP_EQ, 0)) == -EFAULT);
	CHECK(fx(hole, pf | FUTEX_TRYLOCK_PI, 0, (void *)0, 0, 0) == -EFAULT);
	CHECK(fx(hole, pf | FUTEX_UNLOCK_PI, 0, (void *)0, 0, 0) == -EFAULT);
	CHECK(fx((word *)((char *)&w[0] + 1), pf | FUTEX_WAIT, 0, (void *)0, 0, 0) == -EINVAL);
	CHECK(fx(&w[0], pf | FUTEX_WAKE_OP, 0, (void *)0, (word *)((char *)&w[0] + 2), FOP(OP_SET, 1, CMP_EQ, 0)) == -EINVAL);
}

/* a child that exits holding a robust futex, and one with a clear_child_tid
   word: the kernel marks the first and zeroes the second */
static void at_exit(word *s)
{
	unsigned long *head = (unsigned long *)s; /* next, offset, pending; a list node follows */
	word *rw = (word *)&head[5], *ct = (word *)&head[8];
	int st, i;
	pid_t k;

	for (i = 0; i < 3; i++) {
		unsigned flag = i == 1 ? WAITERS : 0;

		head[0] = (unsigned long)&head[4];
		head[1] = 8;
		head[2] = 0;
		head[4] = (unsigned long)head;
		*rw = 0;
		*ct = 0xdead;
		k = fork();
		if (k == 0) {
			*rw = (unsigned)getpid() | flag;
			rawsys6(ROBUST_NR, (long)head, 24, 0, 0, 0, 0);
			if (i == 2)
				rawsys6(TIDADDR_NR, (long)ct, 0, 0, 0, 0, 0);
			_exit(0);
		}
		CHECK(k > 0 && waitpid(k, &st, 0) == k && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		CHECK(*rw == (OWNER_DIED | flag));
		CHECK(*ct == (i == 2 ? 0U : 0xdeadU));
	}
}

int main(void)
{
	word *s = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (s == MAP_FAILED)
		return 1;
	ops();
	wake_op_cmp(s);
	pi(s);
	faults();
	at_exit(s);
	if (failures == 0)
		printf("k_futex: ok\n");
	return failures != 0;
}
