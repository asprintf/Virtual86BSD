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

#define _DEFAULT_SOURCE
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/resource.h>
#include <errno.h>
#include <sched.h>
#include "syscall.h"
#include "libc.h"

#define VER  202405L
#define RLIM(r) (-0x10000 - (r))   /* entry codes that query a resource limit */
#define CPUS_CONF (-0x20000)
#define CPUS_ONLN (-0x20001)
#define PAGESZ    (-0x20002)
#define SPORADIC  (-0x20003)
#define SSREPL    (-0x20004)
#define TYM       (-0x20005)

static const long values[] = {
	[_SC_2_C_BIND] = VER, [_SC_2_C_DEV] = VER, [_SC_2_CHAR_TERM] = 1,
	[_SC_2_FORT_RUN] = VER, [_SC_2_LOCALEDEF] = VER, [_SC_2_SW_DEV] = VER,
	[_SC_2_UPE] = VER, [_SC_2_VERSION] = VER,
	[_SC_ADVISORY_INFO] = VER, [_SC_AIO_LISTIO_MAX] = -1, [_SC_AIO_MAX] = -1,
	[_SC_AIO_PRIO_DELTA_MAX] = 20, [_SC_ARG_MAX] = -2 /* computed */,
	[_SC_ASYNCHRONOUS_IO] = VER, [_SC_ATEXIT_MAX] = ATEXIT_MAX, [_SC_BARRIERS] = VER,
	[_SC_BC_BASE_MAX] = BC_BASE_MAX, [_SC_BC_DIM_MAX] = BC_DIM_MAX,
	[_SC_BC_SCALE_MAX] = BC_SCALE_MAX, [_SC_BC_STRING_MAX] = BC_STRING_MAX,
	[_SC_CHILD_MAX] = RLIM(6 /* RLIMIT_NPROC */), [_SC_CLK_TCK] = 100,
	[_SC_CLOCK_SELECTION] = VER, [_SC_COLL_WEIGHTS_MAX] = COLL_WEIGHTS_MAX,
	[_SC_CPUTIME] = VER, [_SC_DELAYTIMER_MAX] = DELAYTIMER_MAX,
	[_SC_DEVICE_CONTROL] = VER, [_SC_EXPR_NEST_MAX] = EXPR_NEST_MAX,
	[_SC_FSYNC] = VER, [_SC_GETGR_R_SIZE_MAX] = -1, [_SC_GETPW_R_SIZE_MAX] = -1,
	[_SC_HOST_NAME_MAX] = HOST_NAME_MAX, [_SC_IOV_MAX] = IOV_MAX, [_SC_IPV6] = VER,
	[_SC_JOB_CONTROL] = 1, [_SC_LINE_MAX] = LINE_MAX,
	[_SC_LOGIN_NAME_MAX] = LOGIN_NAME_MAX, [_SC_MAPPED_FILES] = VER,
	[_SC_MEMLOCK] = VER, [_SC_MEMLOCK_RANGE] = VER, [_SC_MEMORY_PROTECTION] = VER,
	[_SC_MESSAGE_PASSING] = VER, [_SC_MONOTONIC_CLOCK] = VER, [_SC_MQ_OPEN_MAX] = -1,
	[_SC_MQ_PRIO_MAX] = MQ_PRIO_MAX, [_SC_NGROUPS_MAX] = NGROUPS_MAX,
	[_SC_NPROCESSORS_CONF] = CPUS_CONF, [_SC_NPROCESSORS_ONLN] = CPUS_ONLN,
	[_SC_NSIG] = _NSIG, [_SC_OPEN_MAX] = RLIM(RLIMIT_NOFILE), [_SC_PAGE_SIZE] = PAGESZ,
	[_SC_PRIORITIZED_IO] = VER, [_SC_PRIORITY_SCHEDULING] = VER, [_SC_RAW_SOCKETS] = VER,
	[_SC_RE_DUP_MAX] = RE_DUP_MAX, [_SC_READER_WRITER_LOCKS] = VER,
	[_SC_REALTIME_SIGNALS] = VER, [_SC_REGEXP] = 1, [_SC_RTSIG_MAX] = SIGRTMAX - SIGRTMIN + 1,
	[_SC_SAVED_IDS] = 1, [_SC_SEM_NSEMS_MAX] = -1, [_SC_SEM_VALUE_MAX] = SEM_VALUE_MAX,
	[_SC_SEMAPHORES] = VER, [_SC_SHARED_MEMORY_OBJECTS] = VER, [_SC_SHELL] = 1,
	[_SC_SIGQUEUE_MAX] = RLIM(11 /* RLIMIT_SIGPENDING */), [_SC_SPAWN] = VER,
	[_SC_SPIN_LOCKS] = VER, [_SC_SPORADIC_SERVER] = SPORADIC, [_SC_SS_REPL_MAX] = SSREPL,
	[_SC_STREAM_MAX] = -1, [_SC_SYMLOOP_MAX] = SYMLOOP_MAX, [_SC_SYNCHRONIZED_IO] = VER,
	[_SC_THREAD_ATTR_STACKADDR] = VER, [_SC_THREAD_ATTR_STACKSIZE] = VER,
	[_SC_THREAD_CPUTIME] = VER, [_SC_THREAD_DESTRUCTOR_ITERATIONS] = PTHREAD_DESTRUCTOR_ITERATIONS,
	[_SC_THREAD_KEYS_MAX] = PTHREAD_KEYS_MAX, [_SC_THREAD_PRIO_INHERIT] = VER,
	[_SC_THREAD_PRIO_PROTECT] = VER, [_SC_THREAD_PRIORITY_SCHEDULING] = VER,
	[_SC_THREAD_PROCESS_SHARED] = VER, [_SC_THREAD_ROBUST_PRIO_INHERIT] = VER,
	[_SC_THREAD_ROBUST_PRIO_PROTECT] = VER, [_SC_THREAD_SAFE_FUNCTIONS] = VER,
	[_SC_THREAD_SPORADIC_SERVER] = SPORADIC, [_SC_THREAD_STACK_MIN] = PTHREAD_STACK_MIN,
	[_SC_THREAD_THREADS_MAX] = -1, [_SC_THREADS] = VER, [_SC_TIMEOUTS] = VER,
	[_SC_TIMER_MAX] = -1, [_SC_TIMERS] = VER, [_SC_TTY_NAME_MAX] = TTY_NAME_MAX,
	[_SC_TYPED_MEMORY_OBJECTS] = TYM, [_SC_TZNAME_MAX] = TZNAME_MAX,
	[_SC_V8_ILP32_OFF32] = -1, [_SC_V8_ILP32_OFFBIG] = -1, [_SC_V8_LP64_OFF64] = 1,
	[_SC_V8_LPBIG_OFFBIG] = 1, [_SC_V7_ILP32_OFF32] = -1, [_SC_V7_ILP32_OFFBIG] = -1,
	[_SC_V7_LP64_OFF64] = 1, [_SC_V7_LPBIG_OFFBIG] = 1, [_SC_VERSION] = VER,
	[_SC_XOPEN_CRYPT] = 1, [_SC_XOPEN_ENH_I18N] = 1, [_SC_XOPEN_REALTIME] = 1,
	[_SC_XOPEN_REALTIME_THREADS] = 1, [_SC_XOPEN_SHM] = 1, [_SC_XOPEN_UNIX] = 1,
	[_SC_XOPEN_UUCP] = 1, [_SC_XOPEN_VERSION] = 800,
	/* of older editions (share/doc/legacy.md) */
	[_SC_2_C_VERSION] = 199209L, [_SC_PASS_MAX] = PASS_MAX,
	[_SC_XOPEN_XCU_VERSION] = 5, [_SC_XBS5_ILP32_OFF32] = -1,
	[_SC_XBS5_ILP32_OFFBIG] = -1, [_SC_XBS5_LP64_OFF64] = 1,
	[_SC_XBS5_LPBIG_OFFBIG] = 1, [_SC_XOPEN_LEGACY] = 1, [_SC_2_FORT_DEV] = -1,
	[_SC_V6_ILP32_OFF32] = -1, [_SC_V6_ILP32_OFFBIG] = -1, [_SC_V6_LP64_OFF64] = 1,
	[_SC_V6_LPBIG_OFFBIG] = 1, [_SC_2_PBS] = -1, [_SC_2_PBS_ACCOUNTING] = -1,
	[_SC_2_PBS_CHECKPOINT] = -1, [_SC_2_PBS_LOCATE] = -1, [_SC_2_PBS_MESSAGE] = -1,
	[_SC_2_PBS_TRACK] = -1, [_SC_TRACE] = -1, [_SC_TRACE_EVENT_FILTER] = -1,
	[_SC_TRACE_EVENT_NAME_MAX] = -1, [_SC_TRACE_INHERIT] = -1, [_SC_TRACE_LOG] = -1,
	[_SC_TRACE_NAME_MAX] = -1, [_SC_TRACE_SYS_MAX] = -1, [_SC_TRACE_USER_EVENT_MAX] = -1,
	[_SC_XOPEN_STREAMS] = -1,
};

/* Count the CPUs in a kernel cpu mask. */
static long ncpus(void)
{
	unsigned char set[128];
	long r = __syscall3(SYS_sched_getaffinity, 0, sizeof set, set), n = 0, i;
	if (r <= 0) return 1;
	for (i = 0; i < r; i++) { unsigned c = set[i]; for (; c; c &= c - 1) n++; }
	return n ? n : 1;
}

long sysconf(int name)
{
	long v;
	if ((unsigned)name >= sizeof values / sizeof *values) { errno = EINVAL; return -1; }
	v = values[name];
	if (v >= -1) return v;
	switch (v) {
	case -2: {   /* ARG_MAX: a quarter of the stack limit, as Linux allows */
		struct rlimit l;
		if (getrlimit(RLIMIT_STACK, &l) || l.rlim_cur == RLIM_INFINITY) return 2097152;
		v = (long)(l.rlim_cur / 4);
		return v < _POSIX_ARG_MAX ? _POSIX_ARG_MAX : v;
	}
	case CPUS_CONF: case CPUS_ONLN: return ncpus();
	case PAGESZ: return (long)__libc.page_size;
	/* the sporadic server: where the kernel takes the policy (V86BSD's
	   kernel; Linux has no such policy and says EINVAL) */
	case SPORADIC: return __syscall1(SYS_sched_get_priority_max, SCHED_SPORADIC) > 0 ? VER : -1;
	case SSREPL: return __syscall1(SYS_sched_get_priority_max, SCHED_SPORADIC) > 0 ? 32 : -1;
	/* typed memory: where the kernel offers it (V86BSD's: /dev/tymctl) */
	case TYM: return __syscall2(SYS_access, "/dev/tymctl", 0) == 0 ? VER : -1;
	default: {
		struct rlimit l;
		if (getrlimit(-0x10000 - (int)v, &l)) return -1;
		return l.rlim_cur == RLIM_INFINITY ? -1 : (long)l.rlim_cur;
	}
	}
}
