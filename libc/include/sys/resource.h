#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H
#include <bits/features.h>
#define __NEED_id_t
#define __NEED_time_t
#define __NEED_suseconds_t
#define __NEED_struct_timeval
#include <bits/alltypes.h>

typedef unsigned long rlim_t;

#define RLIM_INFINITY  (~0UL)
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)

#define RLIMIT_CPU    0
#define RLIMIT_FSIZE  1
#define RLIMIT_DATA   2
#define RLIMIT_STACK  3
#define RLIMIT_CORE   4
#define RLIMIT_NOFILE 7
#define RLIMIT_AS     9

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

/* The kernel's layout. */
struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
	long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss, ru_minflt, ru_majflt, ru_nswap,
	     ru_inblock, ru_oublock, ru_msgsnd, ru_msgrcv, ru_nsignals, ru_nvcsw, ru_nivcsw;
};

__BEGIN_DECLS
int getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
#if __XSI_VISIBLE
int getpriority(int, id_t);
int setpriority(int, id_t, int);
int getrusage(int, struct rusage *);
#endif
__END_DECLS
#endif
