#ifndef _SCHED_H
#define _SCHED_H
#include <bits/features.h>
#define __NEED_pid_t
#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

struct sched_param {
	int sched_priority;
};

#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2

__BEGIN_DECLS
int sched_get_priority_max(int);
int sched_get_priority_min(int);
int sched_getparam(pid_t, struct sched_param *);
int sched_getscheduler(pid_t);
int sched_rr_get_interval(pid_t, struct timespec *);
int sched_setparam(pid_t, const struct sched_param *);
int sched_setscheduler(pid_t, int, const struct sched_param *);
int sched_yield(void);
__END_DECLS

#endif
