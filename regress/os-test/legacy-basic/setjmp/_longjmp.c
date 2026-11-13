/*[SUSv4]*/
/* Test whether _longjmp returns to _setjmp, leaving the signal mask. */

#include <setjmp.h>
#include <signal.h>

#include "../basic.h"

static jmp_buf env;

int main(void)
{
	sigset_t set, now;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);
	volatile int times = 0;
	int r = _setjmp(env);
	times++;
	if ( r == 0 )
	{
		sigaddset(&set, SIGUSR1);
		sigprocmask(SIG_BLOCK, &set, NULL);
		_longjmp(env, 0);	/* 0 becomes 1 */
	}
	if ( r != 1 || times != 2 )
		errx(1, "_setjmp returned %d the %dth time", r, times);
	sigprocmask(SIG_BLOCK, NULL, &now);
	if ( !sigismember(&now, SIGUSR1) )
		errx(1, "_longjmp changed the signal mask");
	return 0;
}
