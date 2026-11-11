/* getpass(3) (misc/getpass.c) on a pseudo-terminal: the prompt is written
 * to the terminal and not the answer, the answer is what was typed (at most
 * 128 characters, the rest of the line dropped, what was typed before the
 * prompt thrown away), the terminal's echo is on
 * again afterwards, end of file gives "", a process without a terminal reads
 * stdin and writes the prompt to stderr, and a signal during the read ends
 * the process by that signal with the terminal restored. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "out.h"

static int master;
static char slave_name[128];

static void nap(void)
{
	usleep(300000);
}

/* a child that is a session leader on the pseudo-terminal's slave and runs
   getpass(prompt) PASSES times; what it got goes to the pipe, one line per call, then
   "echo=0|1" for the terminal's state after the last */
static pid_t start(int passes, const char *prompt, int *result)
{
	int p[2];
	pid_t pid;
	if (pipe(p) != 0)
		return -1;
	pid = fork();
	if (pid == 0) {
		int i, fd;
		struct termios t;
		close(p[0]);
		setsid();
		fd = open(slave_name, O_RDWR); /* the controlling terminal */
		if (fd < 0)
			_exit(90);
		for (i = 0; i < passes; i++) {
			char *s = getpass(prompt);
			write(p[1], s, strlen(s));
			write(p[1], "\n", 1);
		}
		if (tcgetattr(fd, &t) == 0)
			write(p[1], (t.c_lflag & ECHO) ? "echo=1\n" : "echo=0\n", 7);
		_exit(0);
	}
	close(p[1]);
	*result = p[0];
	return pid;
}

static size_t slurp(int fd, char *buf, size_t size)
{
	size_t n = 0;
	ssize_t r;
	while (n < size - 1 && (r = read(fd, buf + n, size - 1 - n)) > 0)
		n += (size_t)r;
	buf[n] = 0;
	return n;
}

/* what the master end read back from the slave: the terminal's output */
static void drain(char *buf, size_t size)
{
	size_t n = 0;
	ssize_t r;
	int fl = fcntl(master, F_GETFL);
	fcntl(master, F_SETFL, fl | O_NONBLOCK);
	while (n < size - 1 && (r = read(master, buf + n, size - 1 - n)) > 0)
		n += (size_t)r;
	buf[n] = 0;
	fcntl(master, F_SETFL, fl);
}

int main(void)
{
	char out[1024], res[1024], longline[300];
	int rfd, st;
	pid_t pid;

	master = posix_openpt(O_RDWR | O_NOCTTY);
	CHECK(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
	strncpy(slave_name, ptsname(master), sizeof slave_name - 1);

	/* a password: the prompt shows, the answer does not */
	pid = start(1, "Password:", &rfd);
	nap();
	write(master, "secret\n", 7);
	slurp(rfd, res, sizeof res);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	drain(out, sizeof out);
	CHECK(strcmp(res, "secret\necho=1\n") == 0);
	CHECK(strstr(out, "Password:") != NULL);
	CHECK(strstr(out, "secret") == NULL);
	close(rfd);

	/* a long line: 128 characters are kept, the rest of it is dropped */
	memset(longline, 'x', 200);
	longline[200] = '\n';
	pid = start(1, "pw: ", &rfd);
	nap();
	write(master, longline, 201);
	slurp(rfd, res, sizeof res);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(strlen(res) == 128 + 1 + 7); /* 128 x, newline, "echo=1\n" */
	CHECK(memcmp(res, longline, 128) == 0 && res[128] == '\n');
	drain(out, sizeof out);
	close(rfd);

	/* input typed before the prompt is thrown away (the terminal is flushed) */
	write(master, "typed ahead\n", 12);
	nap();
	pid = start(1, "pw: ", &rfd);
	nap();
	write(master, "real\n", 5);
	slurp(rfd, res, sizeof res);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(strcmp(res, "real\necho=1\n") == 0);
	drain(out, sizeof out);
	close(rfd);

	/* an empty line, and the prompt can be empty */
	pid = start(1, "", &rfd);
	nap();
	write(master, "\n", 1);
	slurp(rfd, res, sizeof res);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(strcmp(res, "\necho=1\n") == 0);
	drain(out, sizeof out);
	close(rfd);

	/* end of file (^D at the start of a line): "" */
	pid = start(1, "pw: ", &rfd);
	nap();
	write(master, "\004", 1);
	slurp(rfd, res, sizeof res);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(strcmp(res, "\necho=1\n") == 0);
	drain(out, sizeof out);
	close(rfd);

	/* no controlling terminal: stdin and stderr */
	{
		int in[2], err[2], rp[2];
		pipe(in); pipe(err); pipe(rp);
		pid = fork();
		if (pid == 0) {
			char *s;
			setsid();
			dup2(in[0], 0);
			dup2(err[1], 2);
			close(in[1]); close(err[0]); close(rp[0]);
			s = getpass("Pass: ");
			write(rp[1], s, strlen(s));
			_exit(0);
		}
		close(in[0]); close(err[1]); close(rp[1]);
		write(in[1], "from stdin\nignored\n", 19);
		slurp(rp[0], res, sizeof res);
		slurp(err[0], out, sizeof out);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		CHECK(strcmp(res, "from stdin") == 0);
		CHECK(strcmp(out, "Pass: ") == 0); /* a pipe has no echo to turn off, so no newline */
		close(in[1]); close(err[0]); close(rp[0]);
	}

	/* a signal while it waits: the terminal is as it was, the process dies of it */
	pid = start(1, "pw: ", &rfd);
	nap();
	{
		struct termios t;
		CHECK(tcgetattr(master, &t) == 0 && !(t.c_lflag & ECHO)); /* echo is off while it reads */
	}
	kill(pid, SIGINT);
	CHECK(waitpid(pid, &st, 0) == pid && WIFSIGNALED(st) && WTERMSIG(st) == SIGINT);
	{
		struct termios t;
		CHECK(tcgetattr(master, &t) == 0 && (t.c_lflag & ECHO)); /* and it is on again */
	}
	close(rfd);

	os(failures ? "getpass FAILED\n" : "getpass ok\n");
	oflush();
	return failures != 0;
}
