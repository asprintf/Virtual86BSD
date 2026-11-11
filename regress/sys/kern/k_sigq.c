/* sigqueue() fails with EAGAIN when the signals already queued for the
 * receiver reach the limit that sysconf(_SC_SIGQUEUE_MAX) reports
 * (RLIMIT_SIGPENDING): the kernel gave the limit and did not keep it. */
#define _POSIX_C_SOURCE 202405L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void)
{
	long lim = sysconf(_SC_SIGQUEUE_MAX), i, got;
	sigset_t set;
	union sigval v;
	struct timespec zero = { 0, 0 };
	siginfo_t si;
	pid_t pid = getpid();

	memset(&v, 0, sizeof v);
	sigemptyset(&set);
	sigaddset(&set, SIGRTMIN);
	sigprocmask(SIG_BLOCK, &set, 0);

	printf("SIGQUEUE_MAX %ld\n", lim);
	if (lim < 1 || lim > 100000) {
		printf("no limit to try\n");
		printf("k_sigq ok\n");
		return 0;
	}
	/* up to the limit; Linux counts per user, so a few other queued signals
	 * of the same user (outside this process) may make it a little earlier */
	for (i = 0; i < lim + 10; i++)
		if (sigqueue(pid, SIGRTMIN, v) != 0)
			break;
	printf("%ld queued, then %s\n", i, i < lim + 10 ? strerror(errno) : "no refusal");
	CHECK(i <= lim && i >= lim - 64);
	CHECK(errno == EAGAIN);
	errno = 0;
	CHECK(sigqueue(pid, SIGRTMIN, v) == -1 && errno == EAGAIN);	/* and it stays so */

	/* taking them off makes room again */
	for (got = 0; sigtimedwait(&set, &si, &zero) == SIGRTMIN; got++)
		;
	CHECK(got == i);
	CHECK(sigqueue(pid, SIGRTMIN, v) == 0);
	CHECK(sigtimedwait(&set, &si, &zero) == SIGRTMIN);
	/* the limit is for queued signals: sig 0 only checks, and is not refused */
	CHECK(sigqueue(pid, 0, v) == 0);

	printf(failures ? "k_sigq FAILED\n" : "k_sigq ok\n");
	return failures != 0;
}
