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

#include <time.h>
#include <errno.h>
#include "syscall.h"
#include "pthread_impl.h"

int clock_gettime(clockid_t c, struct timespec *ts) { return (int)__sys2(SYS_clock_gettime, c, ts); }
int clock_settime(clockid_t c, const struct timespec *ts) { return (int)__sys2(SYS_clock_settime, c, ts); }
int clock_getres(clockid_t c, struct timespec *ts) { return (int)__sys2(SYS_clock_getres, c, ts); }

/* clock_nanosleep returns the error number instead of setting errno */
int clock_nanosleep(clockid_t c, int flags, const struct timespec *req, struct timespec *rem)
{
	if (c == CLOCK_THREAD_CPUTIME_ID) return EINVAL;
	long r = __syscall_cp4(SYS_clock_nanosleep, c, flags, req, rem);
	/* the kernel's posix_* convention: the error number is the result */
	if (r == EINTR)
		__testcancel();
	return (int)(r < 0 ? -r : r);
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return (int)__sys_cp2(SYS_nanosleep, req, rem);
}

/* the kernel's CPU-time clock of a process (clock_getcpuclockid2, CPUCLOCK_WHICH_PID) */
int clock_getcpuclockid(pid_t pid, clockid_t *c)
{
	long r = __syscall3(SYS_clock_getcpuclockid2, pid, 0, c);
	return (int)-r;
}
