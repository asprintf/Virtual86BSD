/* A system call given an address in the kernel's half of the address space
 * (or one that is not canonical) must fail with EFAULT: it must not read,
 * write or stop the kernel. */
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef FUTEX_NR
#define FUTEX_NR 202 /* Linux's; the Makefile gives the system's */
#endif
long rawsys6(long nr, long a, long b, long c, long d, long e, long f);

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); failures++; } } while (0)

/* the result of a futex call that must be -EFAULT */
#define FUTEX_FAULTS(...) \
	do { \
		long r_ = rawsys6(FUTEX_NR, __VA_ARGS__); \
		if (r_ != -EFAULT) { printf("FAIL line %d: futex(%s) = %ld\n", __LINE__, #__VA_ARGS__, r_); failures++; } \
	} while (0)

#define FUTEX_WAKE 1
#define FUTEX_WAKE_OP 5
#define FUTEX_PRIVATE 128

/* the kernel's image, all of memory, the first address above the user half, the
   first of the kernel half, the last aligned word */
static const unsigned long kaddr[] = {0xffffffff80100000UL, 0xffff888000000000UL, 0x0000800000000000UL,
                                      0xffff800000000000UL, ~3UL};
/* the ones that are not memory either: a write may not even fault */
static const unsigned long badaddr[] = {0x0000800000000000UL, 0xffff800000000000UL, ~3UL};

int main(void)
{
	int p[2];
	unsigned word = 0;
	struct timespec ts = {0, 10000000};

	if (pipe(p) != 0)
		return 1;
	for (size_t i = 0; i < sizeof kaddr / sizeof *kaddr; i++) {
		unsigned long a = kaddr[i];
		pid_t c;
		int st;

		CHECK(write(p[1], "x", 1) == 1);
		CHECK(read(p[0], (void *)a, 1) == -1 && errno == EFAULT);
		CHECK(read(p[0], &word, 1) == 1);
		CHECK(write(p[1], (void *)a, 4) == -1 && errno == EFAULT);
		CHECK(open((char *)a, O_RDONLY) == -1 && errno == EFAULT);
		CHECK(stat("/", (struct stat *)a) == -1 && errno == EFAULT);
		CHECK(stat((char *)a, &(struct stat){0}) == -1 && errno == EFAULT);
		CHECK(getcwd((char *)a, 64) == NULL && errno == EFAULT);
		CHECK(clock_gettime(CLOCK_REALTIME, (struct timespec *)a) == -1 && errno == EFAULT);
		/* futex: a private one is not looked up by address */
		FUTEX_FAULTS((long)a, FUTEX_PRIVATE, 0, (long)&ts, 0, 0);
		FUTEX_FAULTS((long)a, FUTEX_PRIVATE | FUTEX_WAKE, 1, 0, 0, 0);
		CHECK(rawsys6(FUTEX_NR, (long)&word, FUTEX_PRIVATE | FUTEX_WAKE_OP, 1, 1, (long)&word, 0) >= 0);
		c = fork();
		if (c == 0) {
			char *const *argv = (char *const *)a;

			execve("/bin/true", argv, NULL);
			_exit(errno == EFAULT ? 0 : 1);
		}
		CHECK(c > 0 && waitpid(c, &st, 0) == c && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
	for (size_t i = 0; i < sizeof badaddr / sizeof *badaddr; i++)
		FUTEX_FAULTS((long)&word, FUTEX_PRIVATE | FUTEX_WAKE_OP, 1, 1, (long)badaddr[i], 0);
	if (failures == 0)
		printf("k_uaccess: ok\n");
	return failures != 0;
}
