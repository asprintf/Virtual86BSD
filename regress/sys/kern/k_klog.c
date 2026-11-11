/* k_klog.c - /dev/klog and kern.msgbuf: the kernel's log.  As root, on V86BSD.
 *
 * Checked: the node is 0600 and only root opens it; a read takes what was
 * printed since the last one, each message once, and a message written to
 * /dev/kmsg arrives whole; with nothing new O_NONBLOCK reads fail with EAGAIN,
 * a blocking read waits for the next message and poll() says when there is
 * one; a read into a bad buffer fails with EFAULT and takes nothing; a short
 * read leaves the rest; two descriptors share one position; kern.msgbuf is
 * the text of the whole buffer (not consumed by /dev/klog), from the start of
 * a line, at most kern.msgbufsize bytes, after the buffer wrapped too. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void nap(long ms)
{
	struct timespec ts = { ms / 1000, ms % 1000 * 1000000 };
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

static void say(const char *s)   /* a message: written to /dev/kmsg */
{
	int fd = open("/dev/kmsg", O_WRONLY);
	if (fd < 0 || write(fd, s, strlen(s)) < 0)
		printf("FAIL cannot write /dev/kmsg: %s\n", strerror(errno)), failures++;
	if (fd >= 0)
		close(fd);
}

/* everything there is, until EAGAIN */
static size_t drain(int fd)
{
	char buf[4096];
	size_t total = 0;
	ssize_t n;
	while ((n = read(fd, buf, sizeof buf)) > 0)
		total += (size_t)n;
	return total;
}

static char *msgbuf(size_t *lenp)
{
	int mib[2] = { CTL_KERN, KERN_MSGBUF }, sz = 0;
	size_t len = sizeof sz;
	char *b;
	int m2[2] = { CTL_KERN, KERN_MSGBUFSIZE };
	if (sysctl(m2, 2, &sz, &len, NULL, 0) < 0)
		return NULL;
	b = malloc((size_t)sz + 1);
	len = (size_t)sz;
	if (!b || sysctl(mib, 2, b, &len, NULL, 0) < 0)
		return NULL;
	b[len] = 0;
	*lenp = len;
	return b;
}

int main(void)
{
	struct stat st;
	char buf[8192];
	ssize_t n;
	int fd, fd2, status, bufsize = 0;
	size_t len, l2;
	pid_t pid;
	struct pollfd pfd;
	char *mb;
	int i;

	CHECK(stat("/dev/klog", &st) == 0 && S_ISCHR(st.st_mode) && (st.st_mode & 07777) == 0600);
	CHECK(major(st.st_rdev) == 1 && minor(st.st_rdev) == 13);

	/* only root */
	pid = fork();
	if (pid == 0) {
		if (setgid(65534) < 0 || setuid(65534) < 0)
			_exit(2);
		int f = open("/dev/klog", O_RDONLY);
		_exit(f < 0 && errno == EACCES ? 0 : 1);
	}
	waitpid(pid, &status, 0);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

	fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
	CHECK(fd >= 0);
	if (fd < 0)
		return 1;
	drain(fd);
	n = read(fd, buf, sizeof buf);
	CHECK(n < 0 && errno == EAGAIN);
	pfd.fd = fd;
	pfd.events = POLLIN;
	CHECK(poll(&pfd, 1, 0) == 0);

	/* a message arrives, once */
	say("k_klog one\n");
	CHECK(poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN));
	n = read(fd, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("k_klog one\n") && memcmp(buf, "k_klog one\n", (size_t)n) == 0);
	n = read(fd, buf, sizeof buf);
	CHECK(n < 0 && errno == EAGAIN);

	/* several at once, in order */
	say("k_klog a\n");
	say("k_klog b\n");
	say("k_klog c\n");
	n = read(fd, buf, sizeof buf);
	CHECK(n == 27 && memcmp(buf, "k_klog a\nk_klog b\nk_klog c\n", 27) == 0);

	/* a short read leaves the rest */
	say("k_klog 0123456789\n");
	n = read(fd, buf, 4);
	CHECK(n == 4 && memcmp(buf, "k_kl", 4) == 0);
	n = read(fd, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("og 0123456789\n") && memcmp(buf, "og 0123456789\n", (size_t)n) == 0);

	/* a bad buffer takes nothing */
	say("k_klog efault\n");
	n = read(fd, (void *)8, 10);
	CHECK(n < 0 && errno == EFAULT);
	n = read(fd, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("k_klog efault\n") && memcmp(buf, "k_klog efault\n", (size_t)n) == 0);

	/* two descriptors share one position */
	fd2 = open("/dev/klog", O_RDONLY | O_NONBLOCK);
	CHECK(fd2 >= 0);
	say("k_klog shared\n");
	n = read(fd2, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("k_klog shared\n"));
	n = read(fd, buf, sizeof buf);
	CHECK(n < 0 && errno == EAGAIN);
	close(fd2);
	say("k_klog after close\n");
	n = read(fd, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("k_klog after close\n"));
	close(fd);

	/* a blocking read waits; so does poll */
	fd = open("/dev/klog", O_RDONLY);
	CHECK(fd >= 0);
	{
		int fl = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
		drain(fd);
		fcntl(fd, F_SETFL, fl);
	}
	pid = fork();
	if (pid == 0) {
		nap(300);
		say("k_klog wake\n");
		_exit(0);
	}
	{
		struct timespec a, b;
		clock_gettime(CLOCK_MONOTONIC, &a);
		n = read(fd, buf, sizeof buf);
		clock_gettime(CLOCK_MONOTONIC, &b);
		CHECK(n == (ssize_t)strlen("k_klog wake\n") && memcmp(buf, "k_klog wake\n", (size_t)n) == 0);
		CHECK((b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000 >= 200);
	}
	waitpid(pid, &status, 0);
	pid = fork();
	if (pid == 0) {
		nap(300);
		say("k_klog poll\n");
		_exit(0);
	}
	pfd.fd = fd;
	pfd.events = POLLIN;
	CHECK(poll(&pfd, 1, 10000) == 1 && (pfd.revents & POLLIN));
	waitpid(pid, &status, 0);
	n = read(fd, buf, sizeof buf);
	CHECK(n == (ssize_t)strlen("k_klog poll\n"));
	/* a signal ends the wait */
	pid = fork();
	if (pid == 0) {
		alarm(1);
		n = read(fd, buf, sizeof buf);
		_exit(99);   /* not reached: SIGALRM ends the process */
	}
	waitpid(pid, &status, 0);
	CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM);
	close(fd);

	/* kern.msgbuf: the whole buffer, klog did not take it away from it */
	say("k_klog in the buffer\n");
	mb = msgbuf(&len);
	CHECK(mb != NULL);
	if (mb) {
		CHECK(len > 0 && mb[len - 1] == '\n');
		CHECK(strstr(mb, "k_klog in the buffer\n") != NULL);
		CHECK(strstr(mb, "k_klog wake\n") != NULL);
		free(mb);
	}
	{
		int m2[2] = { CTL_KERN, KERN_MSGBUFSIZE };
		size_t l = sizeof bufsize;
		CHECK(sysctl(m2, 2, &bufsize, &l, NULL, 0) == 0 && bufsize == 65536);
	}
	{
		/* a read that is too small for the text is an error (ENOMEM) */
		int mib[2] = { CTL_KERN, KERN_MSGBUF };
		char small[4];
		size_t l = sizeof small;
		CHECK(sysctl(mib, 2, small, &l, NULL, 0) < 0 && errno == ENOMEM);
		l = 0;
		CHECK(sysctl(mib, 2, NULL, &l, NULL, 0) == 0 && l > 0);
	}

	/* the buffer wraps: what is left is whole lines, the last ones */
	fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
	drain(fd);
	for (i = 0; i < 700; i++) {
		char line[128];
		snprintf(line, sizeof line, "k_klog fill %04d xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n", i);
		say(line);
	}
	mb = msgbuf(&len);
	CHECK(mb != NULL);
	if (mb) {
		CHECK(len <= 65536 && len > 60000);
		CHECK(strncmp(mb, "k_klog fill ", 12) == 0 || strncmp(mb, "k_klog ", 7) == 0);   /* a whole line first */
		if (strncmp(mb, "k_klog ", 7) != 0)
			printf("first bytes: [%.60s] len %zu\n", mb, len);
		CHECK(strstr(mb, "k_klog fill 0699 ") != NULL);
		CHECK(strstr(mb, "k_klog fill 0000 ") == NULL);   /* the first ones are gone */
		free(mb);
	}
	/* the reader of klog skips what it lost and goes on from the oldest byte left */
	l2 = drain(fd);
	CHECK(l2 > 0 && l2 <= 65536);
	n = read(fd, buf, sizeof buf);
	CHECK(n < 0 && errno == EAGAIN);
	close(fd);

	if (failures == 0)
		printf("k_klog: ok\n");
	return failures != 0;
}
