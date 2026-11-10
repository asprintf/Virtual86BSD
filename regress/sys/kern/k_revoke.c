/* k_revoke.c - revoke(2): a device node and a pseudo-terminal.  As root, on
   V86BSD (the call is native).
 *
 * What is checked: the files that have the device open become dead (read:
 * EIO, or end-of-file on a terminal; write: EIO; ioctl: EBADF; poll: POLLHUP),
 * in this process, in another one, and through a descriptor that was dup()ed;
 * close works; a new open is a live file; a reader sleeping in the terminal
 * wakes up with end-of-file; the session of the terminal gets SIGHUP; the
 * errors (ENOENT, EINVAL for a file that is not a device, EFAULT, EPERM for
 * who does not own the device). */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <termios.h>
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

/* run fn in a child; its exit status */
static int in_child(int (*fn)(void *), void *arg)
{
	pid_t pid = fork();
	int st;
	if (pid == 0)
		_exit(fn(arg));
	if (pid < 0 || waitpid(pid, &st, 0) != pid || !WIFEXITED(st))
		return -1;
	return WEXITSTATUS(st);
}

static const char *node = "/tmp/k_revoke_zero";

static int as_nobody_revoke(void *arg)
{
	(void)arg;
	if (setgid(65534) != 0 || setuid(65534) != 0)
		return 9;
	errno = 0;
	return revoke(node) == -1 && errno == EPERM ? 0 : 1;
}

static int as_owner_revoke(void *arg)
{
	(void)arg;
	if (setgid(65534) != 0 || setuid(65534) != 0)
		return 9;
	return revoke(node) == 0 ? 0 : 1;
}

/* the other process: opens the node, tells us, waits, then reads a dead file */
static int other_process(void *arg)
{
	int *p = arg; /* p[0..1] ready pipe, p[2..3] go pipe */
	char c;
	int fd = open(node, O_RDWR);
	if (fd < 0)
		return 2;
	if (write(p[1], "x", 1) != 1 || read(p[2], &c, 1) != 1)
		return 3;
	errno = 0;
	return read(fd, &c, 1) == -1 && errno == EIO ? 0 : 1;
}

/* a pty slave reader that sleeps in read() until the revoke: end-of-file */
static int sleeping_reader(void *arg)
{
	void **a = arg;
	int fd = open((const char *)a[0], O_RDWR | O_NOCTTY);
	char c;
	if (fd < 0)
		return 2;
	if (write(*(int *)a[1], "x", 1) != 1)
		return 3;
	return read(fd, &c, 1) == 0 ? 0 : 1;
}

/* a session leader whose controlling terminal is the slave: SIGHUP kills it */
static int session_holder(void *arg)
{
	void **a = arg;
	char c;
	setsid();
	int fd = open((const char *)a[0], O_RDWR); /* becomes the controlling terminal */
	if (fd < 0)
		return 2;
	if (write(*(int *)a[1], "x", 1) != 1)
		return 3;
	read(fd, &c, 1); /* sleeps; SIGHUP ends us */
	return 4;
}

int main(void)
{
	char c[8], snbuf[64];
	int fd, fd2, fd3, p[4];
	struct pollfd pf;
	struct termios tio;
	struct stat st;

	alarm(120); /* a hang is a failure, not a stuck test */

	if (getuid() != 0) { /* the kernel tests are run as root; a skip would pass silently */
		printf("FAIL k_revoke needs root\n");
		return 1;
	}

	/* ---- a device node: /dev/zero's numbers ---- */
	unlink(node);
	CHECK(mknod(node, S_IFCHR | 0666, makedev(1, 5)) == 0);
	fd = open(node, O_RDWR);
	CHECK(fd >= 0);
	memset(c, 1, sizeof c);
	CHECK(read(fd, c, 4) == 4 && c[0] == 0);
	fd2 = dup(fd);
	CHECK(fd2 >= 0);
	CHECK(pipe(p) == 0 && pipe(p + 2) == 0);
	{
		pid_t other = fork();
		if (other == 0)
			_exit(other_process(p));
		CHECK(other > 0);
		CHECK(read(p[0], c, 1) == 1); /* it has the node open */
		errno = 0;
		CHECK(revoke(node) == 0);
		CHECK(write(p[3], "x", 1) == 1);
		int st2 = -1;
		CHECK(waitpid(other, &st2, 0) == other && WIFEXITED(st2) && WEXITSTATUS(st2) == 0);
	}
	errno = 0;
	CHECK(read(fd, c, 4) == -1 && errno == EIO);
	errno = 0;
	CHECK(write(fd, c, 4) == -1 && errno == EIO);
	errno = 0;
	CHECK(read(fd2, c, 4) == -1 && errno == EIO); /* the dup is the same file */
	errno = 0;
	CHECK(ioctl(fd, TCGETS, &tio) == -1 && errno == EBADF);
	pf.fd = fd;
	pf.events = POLLIN;
	pf.revents = 0;
	CHECK(poll(&pf, 1, 0) == 1 && (pf.revents & POLLHUP));
	CHECK(fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)); /* the descriptor is still there */
	fd3 = open(node, O_RDWR); /* a new open is live */
	CHECK(fd3 >= 0 && read(fd3, c, 4) == 4);
	CHECK(close(fd) == 0 && close(fd2) == 0 && close(fd3) == 0);
	CHECK(revoke(node) == 0); /* nothing open: fine */

	/* ---- errors ---- */
	errno = 0;
	CHECK(revoke("/nonexistent/k_revoke") == -1 && errno == ENOENT);
	errno = 0;
	CHECK(revoke("/tmp") == -1 && errno == EINVAL); /* a directory */
	{
		const char *reg = "/tmp/k_revoke_reg";
		int r = open(reg, O_CREAT | O_WRONLY, 0644);
		CHECK(r >= 0);
		close(r);
		errno = 0;
		CHECK(revoke(reg) == -1 && errno == EINVAL); /* a regular file */
		unlink(reg);
	}
	errno = 0;
	CHECK(revoke((const char *)1) == -1 && errno == EFAULT);
	CHECK(in_child(as_nobody_revoke, NULL) == 0); /* not the owner: EPERM */
	CHECK(chown(node, 65534, 65534) == 0);
	CHECK(in_child(as_owner_revoke, NULL) == 0); /* the owner may */
	unlink(node);

	/* ---- a pseudo-terminal ---- */
	{
		int m = posix_openpt(O_RDWR | O_NOCTTY);
		CHECK(m >= 0 && grantpt(m) == 0 && unlockpt(m) == 0);
		const char *sn = ptsname(m);
		CHECK(sn != NULL);
		snprintf(snbuf, sizeof snbuf, "%s", sn ? sn : "/dev/null");
		int s = open(snbuf, O_RDWR | O_NOCTTY);
		CHECK(s >= 0 && tcgetattr(s, &tio) == 0);

		/* a reader sleeping in read() is woken with end-of-file */
		int rp[2];
		void *a[2] = { snbuf, &rp[1] };
		CHECK(pipe(rp) == 0);
		pid_t rd = fork();
		if (rd == 0)
			_exit(sleeping_reader(a));
		CHECK(rd > 0 && read(rp[0], c, 1) == 1);
		nap(300); /* it is in read() now */
		CHECK(revoke(snbuf) == 0);
		int st2 = -1;
		CHECK(waitpid(rd, &st2, 0) == rd && WIFEXITED(st2) && WEXITSTATUS(st2) == 0);

		/* this process's own descriptor: a hung-up terminal */
		CHECK(read(s, c, 1) == 0);
		errno = 0;
		CHECK(write(s, "x", 1) == -1 && errno == EIO);
		errno = 0;
		CHECK(tcgetattr(s, &tio) == -1 && errno == EBADF);
		CHECK(close(s) == 0);
		int s2 = open(snbuf, O_RDWR | O_NOCTTY); /* a new open is live */
		CHECK(s2 >= 0 && tcgetattr(s2, &tio) == 0);
		close(s2);
		close(m);
	}

	/* ---- the session of a terminal is hung up ---- */
	{
		int m = posix_openpt(O_RDWR | O_NOCTTY);
		CHECK(m >= 0 && grantpt(m) == 0 && unlockpt(m) == 0);
		const char *sn = ptsname(m);
		CHECK(sn != NULL);
		snprintf(snbuf, sizeof snbuf, "%s", sn ? sn : "/dev/null");
		int hp[2];
		void *a[2] = { snbuf, &hp[1] };
		CHECK(pipe(hp) == 0);
		pid_t h = fork();
		if (h == 0)
			_exit(session_holder(a));
		CHECK(h > 0 && read(hp[0], c, 1) == 1);
		nap(300);
		CHECK(revoke(snbuf) == 0);
		int st2 = -1;
		CHECK(waitpid(h, &st2, 0) == h && WIFSIGNALED(st2) && WTERMSIG(st2) == SIGHUP);
		close(m);
	}

	if (failures == 0)
		printf("k_revoke ok\n");
	return failures != 0;
}
