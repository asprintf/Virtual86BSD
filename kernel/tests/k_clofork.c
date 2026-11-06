/* k_clofork: close-on-fork done by the kernel.  The library must hand the
 * flags to the kernel (the raw F_GETFD shows FD_CLOFORK), and the kernel
 * must close the descriptors in fork() and posix_spawn() children, keep
 * them across exec with FD_CLOFORK cleared (interpretation 1851, TC1), and
 * keep O_CLOFORK out of the file status flags. */
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

long rawgetfd(int fd);                  /* k_clofork_s.s */

/* in a child: is fd open? */
static int open_in_child(int fd)
{
	int st;
	pid_t p = fork();
	if (p == 0) _exit(fcntl(fd, F_GETFD) >= 0);
	waitpid(p, &st, 0);
	return WEXITSTATUS(st);
}

static int open_in_spawn(int fd)
{
	char arg[16], *av[] = { "k_clofork", "-q", arg, 0 };
	pid_t p; int st;
	snprintf(arg, sizeof arg, "%d", fd);
	if (posix_spawn(&p, "/proc/self/exe", 0, 0, av, environ)) return -1;
	waitpid(p, &st, 0);
	return WEXITSTATUS(st);
}

/* FD_CLOFORK set on fd in a process that then execs: the flags it has
   in the new program (2: closed) */
static int flags_after_exec(int fd)
{
	char arg[16], *av[] = { "k_clofork", "-f", arg, 0 };
	pid_t p; int st;
	snprintf(arg, sizeof arg, "%d", fd);
	if ((p = fork()) == 0) {
		fcntl(fd, F_SETFD, FD_CLOFORK);
		execv("/proc/self/exe", av);
		_exit(9);
	}
	waitpid(p, &st, 0);
	return WEXITSTATUS(st);
}

int main(int argc, char **argv)
{
	int fd, fd2, fds[2], sv[2];
	if (argc == 3 && !strcmp(argv[1], "-q"))
		return fcntl(atoi(argv[2]), F_GETFD) >= 0;
	if (argc == 3 && !strcmp(argv[1], "-f")) {
		long r = rawgetfd(atoi(argv[2]));
		return r < 0 ? 2 : (int)r;
	}

	fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOFORK);
	CHECK(fd >= 0 && rawgetfd(fd) == FD_CLOFORK);           /* native */
	CHECK(!(fcntl(fd, F_GETFL) & O_CLOFORK));
	CHECK(open_in_child(fd) == 0);
	CHECK(open_in_spawn(fd) == 0);

	fd2 = dup(fd);
	CHECK(rawgetfd(fd2) == 0 && open_in_child(fd2) == 1 && open_in_spawn(fd2) == 1);
	CHECK(fcntl(fd2, F_SETFD, FD_CLOFORK | FD_CLOEXEC) == 0 && rawgetfd(fd2) == 3);
	CHECK(open_in_child(fd2) == 0);
	CHECK(fcntl(fd2, F_SETFD, FD_CLOEXEC) == 0 && open_in_child(fd2) == 1);
	CHECK(open_in_spawn(fd2) == 0);                       /* closed by exec */
	close(fd2);
	fd2 = dup(fd);
	CHECK(flags_after_exec(fd2) == 0);                    /* kept, FD_CLOFORK cleared */
	close(fd2);

	fd2 = fcntl(fd, F_DUPFD_CLOFORK, 40);
	CHECK(fd2 >= 40 && rawgetfd(fd2) == FD_CLOFORK && open_in_child(fd2) == 0);
	CHECK(dup3(fd, fd2, O_CLOFORK | O_CLOEXEC) == fd2 && rawgetfd(fd2) == 3);
	CHECK(dup2(fd, fd2) == fd2 && rawgetfd(fd2) == 0);    /* dup2 clears */

	CHECK(pipe2(fds, O_CLOFORK) == 0 && rawgetfd(fds[0]) == FD_CLOFORK && rawgetfd(fds[1]) == FD_CLOFORK);
	CHECK(open_in_child(fds[1]) == 0);

	CHECK(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOFORK, 0, sv) == 0 && rawgetfd(sv[0]) == FD_CLOFORK);
	{	/* SCM_RIGHTS with MSG_CMSG_CLOFORK */
		char c = 'x', buf[CMSG_SPACE(sizeof(int))];
		struct iovec iov = { &c, 1 };
		struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = buf, .msg_controllen = sizeof buf };
		struct cmsghdr *cm = CMSG_FIRSTHDR(&m);
		int got;
		cm->cmsg_level = SOL_SOCKET; cm->cmsg_type = SCM_RIGHTS; cm->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cm), &fds[0], sizeof(int));
		CHECK(sendmsg(sv[0], &m, 0) == 1);
		CHECK(recvmsg(sv[1], &m, MSG_CMSG_CLOFORK) == 1);
		memcpy(&got, CMSG_DATA(CMSG_FIRSTHDR(&m)), sizeof got);
		CHECK(rawgetfd(got) == FD_CLOFORK && open_in_child(got) == 0);
	}
	printf("k_clofork: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
