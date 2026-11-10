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

/* <sched.h>: the process's scheduling is that of its main thread (the
 * kernel schedules threads). */
#include <errno.h>
#include <sched.h>
#include "syscall.h"
#include <string.h>

int sched_yield(void) { return (int)__sys0(SYS_sched_yield); }
int sched_get_priority_max(int p) { return (int)__sys1(SYS_sched_get_priority_max, p); }
int sched_get_priority_min(int p) { return (int)__sys1(SYS_sched_get_priority_min, p); }
/* (the kernel writes all of it for SCHED_SPORADIC, else sched_priority) */
int sched_getparam(pid_t pid, struct sched_param *p)
{
	struct sched_param k = {0};
	if (!p) {	/* as Linux: EINVAL (the kernel never sees the null pointer) */
		errno = EINVAL;
		return -1;
	}
	long r = __syscall2(SYS_sched_getparam, pid, &k);
	if (r < 0)
		return (int)__syscall_ret((unsigned long)r);
	*p = k;
	return 0;
}
int sched_getscheduler(pid_t pid) { return (int)__sys1(SYS_sched_getscheduler, pid); }
int sched_setparam(pid_t pid, const struct sched_param *p) { return (int)__sys2(SYS_sched_setparam, pid, p); }
int sched_rr_get_interval(pid_t pid, struct timespec *t) { return (int)__sys2(SYS_sched_rr_get_interval, pid, t); }

/* POSIX: the former policy on success (Linux returns 0) */
int sched_setscheduler(pid_t pid, int policy, const struct sched_param *p)
{
	long old = __syscall1(SYS_sched_getscheduler, pid);
	if (old < 0)
		return (int)__syscall_ret((unsigned long)old);
	if (__sys3(SYS_sched_setscheduler, pid, policy, p) < 0)
		return -1;
	return (int)old;
}
