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

/* Internal: union sigval and siginfo_t, for <signal.h> and <sys/wait.h>. */
#ifndef _BITS_SIGINFO_H
#define _BITS_SIGINFO_H
#define __NEED_pid_t
#define __NEED_uid_t
#include <bits/alltypes.h>

union sigval {
	int sival_int;
	void *sival_ptr;
};

/* The kernel's siginfo (sys/sys/signal.h). */
typedef struct __siginfo {
	int si_signo;
	int si_errno;
	int si_code;
	pid_t si_pid;
	uid_t si_uid;
	int si_status;
	void *si_addr;
	union sigval si_value;
	union {
		struct { int _trapno; } _fault;
		struct { int _timerid; int _overrun; } _timer;
		struct { int _mqd; } _mesgq;
		struct { long _band; } _poll;
		long __spare__[8];
	} _reason;
} siginfo_t;
#define si_timerid _reason._timer._timerid
#define si_overrun _reason._timer._overrun
#define si_band    _reason._poll._band

#endif
