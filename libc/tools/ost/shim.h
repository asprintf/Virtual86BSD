/* shim.h - for a diagnostic run only: usleep() and gettimeofday(), which
   POSIX removed (2008, 2024) and Shall does not have, made of nanosleep()
   and clock_gettime(), for the os-test tests that still call them. */
#include <time.h>
#include <sys/time.h>
static int ost_usleep(unsigned long us)
{
	struct timespec t = { (time_t)(us / 1000000), (long)(us % 1000000) * 1000 };
	return nanosleep(&t, 0);
}
static int ost_gettimeofday(struct timeval *tv, void *tz)
{
	struct timespec t;
	(void)tz;
	clock_gettime(CLOCK_REALTIME, &t);
	tv->tv_sec = t.tv_sec;
	tv->tv_usec = (suseconds_t)(t.tv_nsec / 1000);
	return 0;
}
#define usleep ost_usleep
#define gettimeofday ost_gettimeofday
