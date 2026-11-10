/* ISO C time functions that need only the kernel. */
#include <time.h>
#include "syscall.h"

time_t time(time_t *t)
{
	struct timespec ts;
	__syscall2(SYS_clock_gettime, CLOCK_REALTIME, &ts);
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}

clock_t clock(void)
{
	struct timespec ts;
	if (__syscall2(SYS_clock_gettime, CLOCK_PROCESS_CPUTIME_ID, &ts)) return (clock_t)-1;
	if (ts.tv_sec > (0x7fffffffffffffffL - 999999) / 1000000) return (clock_t)-1;
	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

double difftime(time_t a, time_t b)
{
	return (double)(a - b);   /* exact unless the difference exceeds 2^53 s */
}

int timespec_get(struct timespec *ts, int base)
{
	if (base != TIME_UTC) return 0;
	if (__syscall2(SYS_clock_gettime, CLOCK_REALTIME, ts)) return 0;
	return base;
}
