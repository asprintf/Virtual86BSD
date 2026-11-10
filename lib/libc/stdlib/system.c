/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
