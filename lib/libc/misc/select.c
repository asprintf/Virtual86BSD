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

#include <sys/select.h>
#include <poll.h>
#include <errno.h>
#include "syscall.h"

int ppoll(struct pollfd fds[], nfds_t n, const struct timespec *__RESTRICT ts,
          const sigset_t *__RESTRICT mask)
{
	struct timespec t;
	if (ts) t = *ts;  /* the kernel updates its argument */
	return (int)__sys_cp5(SYS_ppoll, fds, n, ts ? &t : 0, mask, 8);
}

int poll(struct pollfd fds[], nfds_t n, int ms)
{
	struct timespec t;
	if (ms >= 0) { t.tv_sec = ms / 1000; t.tv_nsec = ms % 1000 * 1000000L; }
	return (int)__sys_cp5(SYS_ppoll, fds, n, ms >= 0 ? &t : 0, 0, 8);
}

int pselect(int n, fd_set *__RESTRICT r, fd_set *__RESTRICT w, fd_set *__RESTRICT e,
            const struct timespec *__RESTRICT ts, const sigset_t *__RESTRICT mask)
{
	struct timespec t;
	struct { const sigset_t *m; unsigned long size; } data = { mask, 8 };
	if (ts) {
		if (ts->tv_sec < 0 || (unsigned long)ts->tv_nsec >= 1000000000UL) { errno = EINVAL; return -1; }
		t = *ts;
	}
	return (int)__sys_cp6(SYS_pselect6, n, r, w, e, ts ? &t : 0, &data);
}

int select(int n, fd_set *__RESTRICT r, fd_set *__RESTRICT w, fd_set *__RESTRICT e,
           struct timeval *__RESTRICT tv)
{
	struct timespec t;
	if (tv) {
		if (tv->tv_sec < 0 || tv->tv_usec < 0) { errno = EINVAL; return -1; }
		t.tv_sec = tv->tv_sec + tv->tv_usec / 1000000;
		t.tv_nsec = tv->tv_usec % 1000000 * 1000;
	}
	return pselect(n, r, w, e, tv ? &t : 0, 0);
}
