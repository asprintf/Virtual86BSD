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

#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <sys/wait.h>
#include "spawn_impl.h"
#include "syscall.h"
#include "pthread_impl.h"
#include "clofork.h"
#include "libc.h"

int __sigaction(int, const struct sigaction *, struct sigaction *);
int __execvpe(const char *, char *const [], char *const []);

/* The child: attributes, file actions, exec. Errors go to fd ep as an int. */
static _Noreturn void child(int ep, const char *path, int usepath, const posix_spawn_file_actions_t *fa,
                            const posix_spawnattr_t *at, char *const argv[], char *const envp[],
                            const unsigned long *omask)
{
	struct sigaction sa;
	struct fa *a;
	int sig, e, fd, maxfd = 0;
	short fl = at ? at->__flags : 0;

	struct __pthread *self = __pthread_self();
	self->tid = (int)__syscall0(SYS_gettid);
	self->next = self->prev = self;
	__clofork_close_all();

	/* no handler of the parent may run here: caught signals, and those in
	 * the default set, become SIG_DFL */
	for (sig = 1; sig < _NSIG; sig++) {
		if (sig >= 32 && sig <= 34) continue;
		if (__sigaction(sig, 0, &sa) < 0) continue;
		if ((fl & POSIX_SPAWN_SETSIGDEF) && sigismember(&at->__def, sig) == 1) ;
		else if (sa.sa_handler == SIG_DFL || sa.sa_handler == SIG_IGN) continue;
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = SIG_DFL;
		__sigaction(sig, &sa, 0);
	}
	/* keep the error pipe out of the way of the file actions */
	if (fa) for (a = fa->__head; a; a = a->next) if (a->fd > maxfd) maxfd = a->fd;
	if (ep <= maxfd) {
		if ((fd = fcntl(ep, F_DUPFD_CLOEXEC, maxfd + 1)) < 0) goto fail;
		close(ep);
		ep = fd;
	}
	if ((fl & POSIX_SPAWN_SETSID) && setsid() < 0) goto fail;
	if ((fl & POSIX_SPAWN_SETPGROUP) && setpgid(0, at->__pgrp) < 0) goto fail;
	if (fl & (POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSCHEDPARAM)) {
		struct sched_param sp;
		posix_spawnattr_getschedparam(at, &sp);
		if (fl & POSIX_SPAWN_SETSCHEDULER) {
			if (sched_setscheduler(0, at->__policy, &sp) < 0) goto fail;
		} else if (sched_setparam(0, &sp) < 0) goto fail;
	}
	if ((fl & POSIX_SPAWN_RESETIDS) && (setgid(getgid()) < 0 || setuid(getuid()) < 0)) goto fail;
	if (fa) for (a = fa->__head; a; a = a->next) {
		switch (a->op) {
		case FA_CLOSE:
			if (close(a->fd) < 0 && errno != EBADF && errno != EINPROGRESS) goto fail;
			break;
		case FA_DUP2:
			if (a->srcfd == a->fd) {
				if ((e = fcntl(a->fd, F_GETFD)) < 0 || fcntl(a->fd, F_SETFD, e & ~FD_CLOEXEC) < 0) goto fail;
			} else if (dup2(a->srcfd, a->fd) < 0) goto fail;
			break;
		case FA_OPEN:
			if ((fd = open(a->path, a->oflag, a->mode)) < 0) goto fail;
			if (fd != a->fd) {
				if (dup2(fd, a->fd) < 0) goto fail;
				close(fd);
			}
			break;
		case FA_CHDIR:
			if (chdir(a->path) < 0) goto fail;
			break;
		case FA_FCHDIR:
			if (fchdir(a->fd) < 0) goto fail;
			break;
		}
	}
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (fl & POSIX_SPAWN_SETSIGMASK) ? (const void *)&at->__mask : (const void *)omask, 0, 8);
	if (usepath) __execvpe(path, argv, envp);
	else execve(path, argv, envp);
fail:
	e = errno;
	while (write(ep, &e, sizeof e) < 0 && errno == EINTR);
	__syscall1(SYS_exit_group, 127);
	for (;;);
}

static int spawn(pid_t *res, const char *path, int usepath, const posix_spawn_file_actions_t *fa,
                 const posix_spawnattr_t *at, char *const argv[], char *const envp[])
{
	unsigned long all = -1UL, old;
	int p[2], e = 0, st;
	long pid;
	ssize_t n;
	if (pipe2(p, O_CLOEXEC) < 0) return errno;
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &all, &old, 8);
	pid = __syscall5(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (!pid) {
		close(p[0]);
		child(p[1], path, usepath, fa, at, argv, envp, &old);
	}
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &old, 0, 8);
	close(p[1]);
	if (pid < 0) { close(p[0]); return (int)-pid; }
	while ((n = read(p[0], &e, sizeof e)) < 0 && errno == EINTR);
	close(p[0]);
	if (n == (ssize_t)sizeof e) {
		while (waitpid((pid_t)pid, &st, 0) < 0 && errno == EINTR);
		return e;
	}
	if (res) *res = (pid_t)pid;
	return 0;
}

int posix_spawn(pid_t *__RESTRICT res, const char *__RESTRICT path, const posix_spawn_file_actions_t *__RESTRICT fa,
                const posix_spawnattr_t *__RESTRICT at, char *const argv[__RESTRICT], char *const envp[__RESTRICT])
{
	return spawn(res, path, 0, fa, at, argv, envp);
}

int posix_spawnp(pid_t *__RESTRICT res, const char *__RESTRICT file, const posix_spawn_file_actions_t *__RESTRICT fa,
                 const posix_spawnattr_t *__RESTRICT at, char *const argv[__RESTRICT], char *const envp[__RESTRICT])
{
	return spawn(res, file, 1, fa, at, argv, envp);
}
