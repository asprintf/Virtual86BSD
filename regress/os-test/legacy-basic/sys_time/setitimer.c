/*[SUSv4]*/
/* Test whether setitimer and getitimer set, give and stop the real-time timer. */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../basic.h"

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static volatile sig_atomic_t got;

static void on_sig(int s)
{
	(void) s;
	got++;
}

int main(void)
{
	struct itimerval v, o;
	memset(&v, 0, sizeof v);
	v.it_value.tv_sec = 5;
	v.it_interval.tv_usec = 250000;
	if ( setitimer(ITIMER_REAL, &v, NULL) != 0 )
		err(1, "setitimer");
	if ( getitimer(ITIMER_REAL, &o) != 0 )
		err(1, "getitimer");
	if ( o.it_value.tv_sec < 4 || o.it_value.tv_sec > 5 ||
	     o.it_interval.tv_sec != 0 || o.it_interval.tv_usec != 250000 )
		errx(1, "getitimer did not give the timer set");
	memset(&v, 0, sizeof v);
	if ( setitimer(ITIMER_REAL, &v, &o) != 0 )
		err(1, "setitimer");
	if ( o.it_value.tv_sec < 4 || o.it_interval.tv_usec != 250000 )
		errx(1, "setitimer did not give the old timer");
	if ( getitimer(ITIMER_REAL, &o) != 0 || o.it_value.tv_sec || o.it_value.tv_usec )
		errx(1, "a zero it_value did not stop the timer");
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGALRM, &sa, NULL);
	v.it_value.tv_usec = 20000;
	v.it_interval.tv_usec = 20000;
	setitimer(ITIMER_REAL, &v, NULL);
	long long t0 = now_ms();
	while ( got < 3 && now_ms() - t0 < 2000 )
		usleep(5000);
	if ( got < 3 )
		errx(1, "the interval did not repeat");
	memset(&v, 0, sizeof v);
	setitimer(ITIMER_REAL, &v, NULL);
	errno = 0;
	if ( getitimer(99, &o) != -1 || errno != EINVAL )
		errx(1, "getitimer(99) was not EINVAL");
	errno = 0;
	if ( setitimer(99, &v, NULL) != -1 || errno != EINVAL )
		errx(1, "setitimer(99) was not EINVAL");
	v.it_value.tv_usec = 1000000;
	errno = 0;
	if ( setitimer(ITIMER_REAL, &v, NULL) != -1 || errno != EINVAL )
		errx(1, "setitimer with 1000000 microseconds was not EINVAL");
	return 0;
}
