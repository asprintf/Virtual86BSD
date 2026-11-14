/* Process 0, kernel_task: it owns the kernel threads, is init's parent and has
 * no files or address space; the kernel threads cannot be signalled. */
#define _POSIX_C_SOURCE 202405L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static long slurp(const char *path, char *buf, size_t n)
{
	int fd = open(path, O_RDONLY);
	long len;

	if (fd < 0)
		return -1;
	len = read(fd, buf, n - 1);
	close(fd);
	if (len >= 0)
		buf[len] = 0;
	return len;
}

/* the names of a directory, as numbers */
static int numbers(const char *dir, int *v, int max)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d)
		return -1;
	while ((e = readdir(d)) != NULL)
		if (e->d_name[0] >= '0' && e->d_name[0] <= '9' && n < max)
			v[n++] = atoi(e->d_name);
	closedir(d);
	return n;
}

int main(void)
{
	char buf[1024], comm[32], state, path[64];
	int pid, ppid, pgrp, sid, n, i, pids[64], tids[64];
	int sawzero = 0, sawone = 0, sawkthread = 0;

	/* /proc/0 is kernel_task: its own parent, group and session */
	CHECK(slurp("/proc/0/stat", buf, sizeof(buf)) > 0);
	CHECK(sscanf(buf, "%d (%31[^)]) %c %d %d %d", &pid, comm, &state, &ppid, &pgrp, &sid) == 6);
	CHECK(pid == 0 && !strcmp(comm, "kernel_task") && ppid == 0 && pgrp == 0 && sid == 0);
	CHECK(slurp("/proc/0/status", buf, sizeof(buf)) > 0 && !strncmp(buf, "Name:\tkernel_task\n", 17));
	CHECK(strstr(buf, "\nPid:\t0\n") && strstr(buf, "\nPPid:\t0\n") && strstr(buf, "\nUid:\t0\t"));

	/* init is its child */
	CHECK(slurp("/proc/1/stat", buf, sizeof(buf)) > 0);
	CHECK(sscanf(buf, "%d (%31[^)]) %c %d", &pid, comm, &state, &ppid) == 4 && pid == 1 && ppid == 0);

	/* the process list starts with it */
	n = numbers("/proc", pids, 64);
	CHECK(n > 1);
	for (i = 0; i < n; i++) {
		sawzero += pids[i] == 0;
		sawone += pids[i] == 1;
	}
	CHECK(sawzero == 1 && sawone == 1);

	/* its threads are the kernel threads (the idle task is thread 0) */
	n = numbers("/proc/0/task", tids, 64);
	CHECK(n >= 3); /* idle, flush, loadavg (and net with a card) */
	for (i = 0; i < n; i++) {
		sawkthread += tids[i] == 0;
		snprintf(path, sizeof(path), "/proc/0/task/%d/stat", tids[i]);
		CHECK(slurp(path, buf, sizeof(buf)) > 0);
		/* they cannot be signalled or given a scheduling policy */
		if (tids[i] > 0) {
			errno = 0;
			CHECK(kill(tids[i], 0) == -1 && errno == ESRCH);
			errno = 0;
			CHECK(kill(tids[i], SIGKILL) == -1 && errno == ESRCH);
			errno = 0;
			CHECK(sched_getscheduler(tids[i]) == -1 && errno == ESRCH);
		}
	}
	CHECK(sawkthread == 1);
	snprintf(buf, sizeof(buf), "Threads:\t%d\n", n);
	{
		char st[1024];
		CHECK(slurp("/proc/0/status", st, sizeof(st)) > 0 && strstr(st, buf));
	}

	/* no program, directory, files or memory */
	CHECK(readlink("/proc/0/exe", buf, sizeof(buf)) == -1 && errno == ENOENT);
	CHECK(readlink("/proc/0/cwd", buf, sizeof(buf)) == -1 && errno == ENOENT);
	CHECK(readlink("/proc/0/root", buf, sizeof(buf)) == -1 && errno == ENOENT);
	CHECK(slurp("/proc/0/cmdline", buf, sizeof(buf)) == 0);
	CHECK(slurp("/proc/0/maps", buf, sizeof(buf)) == 0);
	CHECK(numbers("/proc/0/fd", pids, 64) == 0);

	if (!failures)
		printf("k_kproc ok\n");
	return failures != 0;
}
