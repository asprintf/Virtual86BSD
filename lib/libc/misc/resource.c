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
#include <time.h>
#include "syscall.h"

int getrlimit(int r, struct rlimit *l) { return (int)__sys2(SYS_getrlimit, r, l); }
int setrlimit(int r, const struct rlimit *l) { return (int)__sys2(SYS_setrlimit, r, l); }
int getrusage(int who, struct rusage *u) { return (int)__sys2(SYS_getrusage, who, u); }

/* the kernel returns the nice value itself, which can look like an error */
int getpriority(int which, id_t who)
{
	long err, r = __syscall2_cf(SYS_getpriority, which, (int)who, &err);
	if (err) { errno = (int)err; return -1; }
	return (int)r;
}

long __setnice(int);

int setpriority(int which, id_t who, int prio)
{
	return (int)__sys3(SYS_setpriority, which, (int)who, prio);
}

/* times(): the CPU times from getrusage in ticks of 1/128 s (CLK_TCK), the
 * elapsed ticks from the monotonic clock */
#define TICKS 128
static clock_t ticks(const struct timeval *tv)
{
	return (clock_t)(tv->tv_sec * TICKS + tv->tv_usec * TICKS / 1000000);
}

clock_t times(struct tms *t)
{
	struct rusage ru;
	struct timespec ts;
	if (t) {
		if (getrusage(RUSAGE_SELF, &ru)) return (clock_t)-1;
		t->tms_utime = ticks(&ru.ru_utime);
		t->tms_stime = ticks(&ru.ru_stime);
		if (getrusage(RUSAGE_CHILDREN, &ru)) return (clock_t)-1;
		t->tms_cutime = ticks(&ru.ru_utime);
		t->tms_cstime = ticks(&ru.ru_stime);
	}
	if (__syscall2(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) < 0) return (clock_t)-1;
	return (clock_t)(ts.tv_sec * TICKS + ts.tv_nsec / (1000000000 / TICKS));
}
