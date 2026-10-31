#include <time.h>
#include <errno.h>
#include "syscall.h"

int clock_gettime(clockid_t c, struct timespec *ts) { return (int)__sys2(SYS_clock_gettime, c, ts); }
int clock_settime(clockid_t c, const struct timespec *ts) { return (int)__sys2(SYS_clock_settime, c, ts); }
int clock_getres(clockid_t c, struct timespec *ts) { return (int)__sys2(SYS_clock_getres, c, ts); }

/* clock_nanosleep returns the error number instead of setting errno */
int clock_nanosleep(clockid_t c, int flags, const struct timespec *req, struct timespec *rem)
{
	if (c == CLOCK_THREAD_CPUTIME_ID) return EINVAL;
	return (int)-__syscall_cp4(SYS_clock_nanosleep, c, flags, req, rem);
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return (int)__sys_cp2(SYS_nanosleep, req, rem);
}

int clock_getcpuclockid(pid_t pid, clockid_t *c)
{
	struct timespec ts;
	clockid_t id = (clockid_t)((~(unsigned)pid << 3) | 2);   /* CPUCLOCK_SCHED of pid */
	long r = __syscall2(SYS_clock_getres, id, &ts);
	if (r == -EINVAL) return ESRCH;
	if (r) return (int)-r;
	*c = id;
	return 0;
}
