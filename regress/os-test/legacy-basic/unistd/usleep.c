/*[SUSv3]*/
/* Test whether usleep sleeps, and ends with EINTR for a caught signal. */

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
	long long t0 = now_ms();
	if ( usleep(0) != 0 || now_ms() - t0 >= 20 )
		errx(1, "usleep(0) did not return at once with 0");
	t0 = now_ms();
	if ( usleep(60000) != 0 )
		err(1, "usleep(60000)");
	long long dt = now_ms() - t0;
	if ( dt < 55 || dt >= 500 )
		errx(1, "usleep(60000) slept %lld ms", dt);
	/* a million or more "may fail" with EINVAL; V86BSD sleeps */
	t0 = now_ms();
	if ( usleep(1000000) == 0 )
	{
		dt = now_ms() - t0;
		if ( dt < 990 || dt >= 2500 )
			errx(1, "usleep(1000000) slept %lld ms", dt);
	}
	else if ( errno != EINVAL )
		err(1, "usleep(1000000)");
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGALRM, &sa, NULL);
	struct itimerval v;
	memset(&v, 0, sizeof v);
	v.it_value.tv_usec = 30000;
	setitimer(ITIMER_REAL, &v, NULL);
	t0 = now_ms();
	errno = 0;
	if ( usleep(800000) != -1 || errno != EINTR )
		errx(1, "usleep interrupted by a caught signal was not -1 with EINTR");
	if ( got != 1 || now_ms() - t0 >= 600 )
		errx(1, "the signal did not end the sleep");
	return 0;
}
