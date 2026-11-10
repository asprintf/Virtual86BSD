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

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "lock.h"

int kill(pid_t pid, int sig) { return (int)__sys2(SYS_kill, pid, sig); }
int killpg(pid_t pg, int sig)
{
	if (pg < 0) { errno = EINVAL; return -1; }
	return kill(-pg, sig);
}

/* the library's signals (32-34) cannot be blocked */
int pthread_sigmask(int how, const sigset_t *__RESTRICT set, sigset_t *__RESTRICT old)
{
	long r;
	unsigned long m;
	if (set && (unsigned)how > SIG_SETMASK) return EINVAL;
	if (set) {
		m = set->__bits[0];
		if (how != SIG_UNBLOCK) m &= ~(7UL << 31);
	}
	if (old) memset(old, 0, sizeof *old);
	r = __syscall4(SYS_rt_sigprocmask, how, set ? &m : 0, old, 8);
	return (int)-r;
}

int sigprocmask(int how, const sigset_t *__RESTRICT set, sigset_t *__RESTRICT old)
{
	int r = pthread_sigmask(how, set, old);
	if (r) { errno = r; return -1; }
	return 0;
}

int sigpending(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	return (int)__sys2(SYS_rt_sigpending, set, 8);
}

int sigsuspend(const sigset_t *mask)
{
	return (int)__sys_cp2(SYS_rt_sigsuspend, mask, 8);
}

int sigtimedwait(const sigset_t *__RESTRICT set, siginfo_t *__RESTRICT info,
                 const struct timespec *__RESTRICT ts)
{
	return (int)__sys_cp4(SYS_rt_sigtimedwait, set, info, ts, 8);
}

int sigwaitinfo(const sigset_t *__RESTRICT set, siginfo_t *__RESTRICT info)
{
	long r = __syscall_cp4(SYS_rt_sigtimedwait, set, info, 0, 8);
	return (int)__syscall_ret((unsigned long)r);
}

int sigwait(const sigset_t *__RESTRICT set, int *__RESTRICT sig)
{
	long r;
	do r = __syscall_cp4(SYS_rt_sigtimedwait, set, 0, 0, 8);
	while (r == -EINTR);
	if (r < 0) return (int)-r;
	*sig = (int)r;
	return 0;
}

int sigqueue(pid_t pid, int sig, union sigval value)
{
	siginfo_t si;
	memset(&si, 0, sizeof si);
	si.si_signo = sig;
	si.si_code = SI_QUEUE;
	si.si_value = value;
	si.si_uid = (uid_t)__syscall0(SYS_getuid);
	si.si_pid = (pid_t)__syscall0(SYS_getpid);
	return (int)__sys3(SYS_rt_sigqueueinfo, pid, sig, &si);
}

int sigaltstack(const stack_t *__RESTRICT ss, stack_t *__RESTRICT old)
{
	if (ss && !(ss->ss_flags & SS_DISABLE) && ss->ss_size < MINSIGSTKSZ) { errno = ENOMEM; return -1; }
	if (ss && (ss->ss_flags & ~SS_DISABLE)) { errno = EINVAL; return -1; }
	return (int)__sys2(SYS_sigaltstack, ss, old);
}

/* killlock keeps the thread from exiting meanwhile (its tid becomes 0
 * then); a thread that has ended but is not joined yet: 0, as for a
 * thread that ignores the signal */
int __pthread_kill(pthread_t t, int sig)
{
	unsigned long set;
	int r;
	if ((unsigned)sig >= _NSIG) return EINVAL;
	__block_all_sigs(&set);
	LOCK(&t->killlock);
	r = t->tid ? (int)-__syscall2(SYS_tkill, t->tid, sig) : 0;
	UNLOCK(&t->killlock);
	__restore_sigs(&set);
	return r;
}

int pthread_kill(pthread_t t, int sig)
{
	if ((unsigned)sig - 32 < 3) return EINVAL;
	return __pthread_kill(t, sig);
}
