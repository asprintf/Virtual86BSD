/*[SUSv3]*/
/* Test whether ualarm raises SIGALRM after the time, and returns the time left. */

#include <signal.h>
#include <unistd.h>

#include "../basic.h"

static volatile sig_atomic_t alarms;

static void on_alarm(int sig)
{
	(void) sig;
	alarms++;
}

int main(void)
{
	struct sigaction sa = { 0 };
	sa.sa_handler = on_alarm;
	if ( sigaction(SIGALRM, &sa, NULL) < 0 )
		err(1, "sigaction");
	if ( ualarm(0, 0) != 0 )
		errx(1, "the first ualarm did not return 0");
	ualarm(500000, 0);
	useconds_t left = ualarm(200000, 0);
	if ( left == 0 || left > 500000 )
		errx(1, "ualarm returned %u microseconds left of 500000", (unsigned) left);
	while ( !alarms )
		pause();
	/* with an interval: repeated */
	ualarm(10000, 10000);
	while ( alarms < 4 )
		pause();
	ualarm(0, 0);
	return 0;
}
