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

/* The kernel's scheduling of a thread (rtprio_thread): what the pthread_ functions
 * and the library's own mutexes and threads use.  No POSIX names here. */
#include <errno.h>
#include <sched.h>
#include "pthread_impl.h"
#include "syscall.h"

#define RTP_LOOKUP 0
#define RTP_SET 1
#define RTP_PRIO_REALTIME 2
#define RTP_PRIO_NORMAL 3
#define RTP_PRIO_FIFO 10
#define RTP_PRIO_MAX 31

struct rtprio { unsigned short type, prio; };

/* the kernel's mapping of POSIX priorities (sys/kern/ksched.c): the
 * real-time ones 0-31 are 31 - prio; SCHED_OTHER's are 0 up to what
 * sched_get_priority_max(SCHED_OTHER) says, reversed */
long __thr_setsched(long tid, int policy, int prio)
{
	struct rtprio rtp;
	if (policy == SCHED_FIFO || policy == SCHED_RR) {
		if (prio < 0 || prio > RTP_PRIO_MAX) return -EINVAL;
		rtp.type = policy == SCHED_FIFO ? RTP_PRIO_FIFO : RTP_PRIO_REALTIME;
		rtp.prio = (unsigned short)(RTP_PRIO_MAX - prio);
	} else if (policy == SCHED_OTHER) {
		long max = __syscall1(SYS_sched_get_priority_max, SCHED_OTHER);
		if (max < 0) return max;
		if (prio < 0 || prio > max) return -EINVAL;
		rtp.type = RTP_PRIO_NORMAL;
		rtp.prio = (unsigned short)(max - prio);
	} else
		return -EINVAL;
	return __syscall3(SYS_rtprio_thread, RTP_SET, tid, &rtp);
}

long __thr_getsched(long tid, int *policy, int *prio)
{
	struct rtprio rtp;
	long r = __syscall3(SYS_rtprio_thread, RTP_LOOKUP, tid, &rtp);
	if (r < 0) return r;
	if (rtp.type == RTP_PRIO_FIFO || rtp.type == RTP_PRIO_REALTIME) {
		*policy = rtp.type == RTP_PRIO_FIFO ? SCHED_FIFO : SCHED_RR;
		*prio = RTP_PRIO_MAX - rtp.prio;
	} else {
		long max = __syscall1(SYS_sched_get_priority_max, SCHED_OTHER);
		*policy = SCHED_OTHER;
		*prio = max < 0 ? 0 : rtp.prio > max ? 0 : (int)max - rtp.prio;
	}
	return 0;
}
