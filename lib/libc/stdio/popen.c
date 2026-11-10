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
