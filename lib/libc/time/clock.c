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

/* ISO C time functions that need only the kernel. */
#include <time.h>
#include "syscall.h"

time_t time(time_t *t)
{
	struct timespec ts;
	__syscall2(SYS_clock_gettime, CLOCK_REALTIME, &ts);
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}

clock_t clock(void)
{
	struct timespec ts;
	if (__syscall2(SYS_clock_gettime, CLOCK_PROCESS_CPUTIME_ID, &ts)) return (clock_t)-1;
	if (ts.tv_sec > (0x7fffffffffffffffL - 999999) / 1000000) return (clock_t)-1;
	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

double difftime(time_t a, time_t b)
{
	return (double)(a - b);   /* exact unless the difference exceeds 2^53 s */
}

int timespec_get(struct timespec *ts, int base)
{
	if (base != TIME_UTC) return 0;
	if (__syscall2(SYS_clock_gettime, CLOCK_REALTIME, ts)) return 0;
	return base;
}
