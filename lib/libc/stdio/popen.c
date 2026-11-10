/* popen()/pclose(): the shell runs in a child created with fork();
 * streams of earlier popen() calls are closed in the child (POSIX). */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "stdio_impl.h"
#include "libc.h"
#include "syscall.h"

FILE *popen(const char *cmd, const char *mode)
{
	int p[2], mine, theirs, target;
	pid_t pid;
	FILE *f;
	if ((*mode != 'r' && *mode != 'w') || (mode[1] && mode[1] != 'e')) { errno = EINVAL; return 0; }
	if (pipe2(p, O_CLOEXEC)) return 0;
	mine = *mode == 'r' ? p[0] : p[1];
	theirs = *mode == 'r' ? p[1] : p[0];
	target = *mode == 'r' ? 1 : 0;
	f = __fdopen(mine, *mode == 'r' ? "r" : "w");
	if (!f) { close(p[0]); close(p[1]); return 0; }
	pid = fork();
	if (pid < 0) { fclose(f); close(theirs); return 0; }
	if (!pid) {
		FILE *o;
		for (o = *__ofl_lock(); o; o = o->next)
			if (o->pipe_pid && o != f) __syscall1(SYS_close, o->fd);
		__ofl_unlock();
		if (theirs == target) __syscall3(SYS_fcntl, theirs, F_SETFD, 0);
		else if (dup2(theirs, target) < 0) _exit(127);
		execl("/bin/sh", "sh", "-c", "--", cmd, (char *)0);
		_exit(127);
	}
	close(theirs);
	if (mode[1] != 'e') fcntl(mine, F_SETFD, 0);
	f->pipe_pid = pid;
	return f;
}

int pclose(FILE *f)
{
	int status;
	pid_t pid = f->pipe_pid, r;
	if (!pid) { errno = ECHILD; return -1; }
	fclose(f);
	while ((r = waitpid(pid, &status, 0)) < 0 && errno == EINTR);
	return r < 0 ? -1 : status;
}
