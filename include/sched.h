#ifndef _SCHED_H
#define _SCHED_H
#include <bits/features.h>
#define __NEED_pid_t
#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

/* the sporadic server's members ([SS]/[TSP]) follow sched_priority; the
 * kernel reads them for SCHED_SPORADIC only */
struct sched_param {
	int sched_priority;
	int sched_ss_low_priority;
	struct timespec sched_ss_repl_period;
	struct timespec sched_ss_init_budget;
	int sched_ss_max_repl;
	int __pad;
};

#define SCHED_OTHER    0
#define SCHED_FIFO     1
#define SCHED_RR       2
#define SCHED_SPORADIC 4   /* where the kernel has it: sysconf(_SC_SPORADIC_SERVER) */

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
