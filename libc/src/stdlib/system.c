/* system(): ISO C, so only reserved names; SIGINT and SIGQUIT are ignored
 * and SIGCHLD blocked in the caller while the command runs (POSIX). */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"
#include "libc.h"

int __sigaction(int, const struct sigaction *, struct sigaction *);

int system(const char *cmd)
{
	struct sigaction ign, oint, oquit;
	unsigned long chld = 1UL << (SIGCHLD - 1), omask;
	long pid, r;
	int status;

	if (!cmd) return 1;   /* a shell is available */
	memset(&ign, 0, sizeof ign);
	ign.sa_handler = SIG_IGN;
	__sigaction(SIGINT, &ign, &oint);
	__sigaction(SIGQUIT, &ign, &oquit);
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &chld, &omask, 8);

	pid = __syscall5(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (pid == 0) {
		const char *argv[] = { "sh", "-c", "--", cmd, 0 };
		__sigaction(SIGINT, &oint, 0);
		__sigaction(SIGQUIT, &oquit, 0);
		__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &omask, 0, 8);
		__syscall3(SYS_execve, "/bin/sh", argv, __environ);
		__syscall1(SYS_exit_group, 127);
	}
	status = -1;
	if (pid > 0) {
		while ((r = __syscall4(SYS_wait4, pid, &status, 0, 0)) == -EINTR);
		if (r < 0) status = -1;
	} else {
		errno = (int)-pid;
	}
	__sigaction(SIGINT, &oint, 0);
	__sigaction(SIGQUIT, &oquit, 0);
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &omask, 0, 8);
	return status;
}
