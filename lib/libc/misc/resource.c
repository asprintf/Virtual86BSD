#include <sys/resource.h>
#include <sys/times.h>
#include <errno.h>
#include "syscall.h"

int getrlimit(int r, struct rlimit *l) { return (int)__sys4(SYS_prlimit64, 0, r, 0, l); }
int setrlimit(int r, const struct rlimit *l) { return (int)__sys4(SYS_prlimit64, 0, r, l, 0); }
int getrusage(int who, struct rusage *u) { return (int)__sys2(SYS_getrusage, who, u); }

int getpriority(int which, id_t who)
{
	long r = __syscall2(SYS_getpriority, which, who);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	return 20 - (int)r;   /* the kernel returns 20 - nice */
}

long __setnice(int);

int setpriority(int which, id_t who, int prio)
{
	/* the process itself: every thread (Linux has a nice value per thread) */
	if (which == PRIO_PROCESS && (!who || who == (id_t)__syscall0(SYS_getpid)))
		return (int)__syscall_ret((unsigned long)__setnice(prio));
	return (int)__sys3(SYS_setpriority, which, who, prio);
}

clock_t times(struct tms *t) { return (clock_t)__syscall1(SYS_times, t); }
