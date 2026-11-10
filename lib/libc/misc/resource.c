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

#include <sys/resource.h>
#include <sys/times.h>
#include <errno.h>
#include "syscall.h"

int getrlimit(int r, struct rlimit *l) { return (int)__sys4(SYS_prlimit64, 0, r, 0, l); }
int setrlimit(int r, const struct rlimit *l) { return (int)__sys4(SYS_prlimit64, 0, r, l, 0); }
int getrusage(int who, struct rusage *u) { return (int)__sys2(SYS_getrusage, who, u); }

int getpriority(int which, id_t who)
{
	long r = __syscall2(SYS_getpriority, which, who);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	return 20 - (int)r;   /* the kernel returns 20 - nice */
}

long __setnice(int);

int setpriority(int which, id_t who, int prio)
{
	/* the process itself: every thread (Linux has a nice value per thread) */
	if (which == PRIO_PROCESS && (!who || who == (id_t)__syscall0(SYS_getpid)))
		return (int)__syscall_ret((unsigned long)__setnice(prio));
	return (int)__sys3(SYS_setpriority, which, who, prio);
}

clock_t times(struct tms *t) { return (clock_t)__syscall1(SYS_times, t); }
