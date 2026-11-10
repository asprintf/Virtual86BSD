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

/* The kernel's 128-byte siginfo. */
typedef struct {
	int si_signo, si_errno, si_code;
	union {
		char __pad[128 - 2 * sizeof(int) - sizeof(long)];
		struct {
			union {
				struct { pid_t __pid; uid_t __uid; } __piduid;
				struct { int __timerid; int __overrun; } __timer;
			} __first;
			union {
				union sigval __value;
				struct { int __status; long __utime, __stime; } __sigchld;
			} __second;
		} __si_common;
		struct {
			void *__addr;
			short __addr_lsb;
			union {
				struct { void *__lower, *__upper; } __addr_bnd;
				unsigned __pkey;
			} __first;
		} __sigfault;
		struct { long __band; int __fd; } __sigpoll;
		struct { void *__call_addr; int __syscall; unsigned __arch; } __sigsys;
	} __si_fields;
} siginfo_t;
#define si_pid     __si_fields.__si_common.__first.__piduid.__pid
#define si_uid     __si_fields.__si_common.__first.__piduid.__uid
#define si_status  __si_fields.__si_common.__second.__sigchld.__status
#define si_utime   __si_fields.__si_common.__second.__sigchld.__utime
#define si_stime   __si_fields.__si_common.__second.__sigchld.__stime
#define si_value   __si_fields.__si_common.__second.__value
#define si_addr    __si_fields.__sigfault.__addr
#define si_band    __si_fields.__sigpoll.__band
#define si_fd      __si_fields.__sigpoll.__fd
#define si_timerid __si_fields.__si_common.__first.__timer.__timerid
#define si_overrun __si_fields.__si_common.__first.__timer.__overrun

#endif
