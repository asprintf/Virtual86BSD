/*[SUSv3]*/
/* Test whether setcontext resumes where getcontext was called. */

#include <ucontext.h>

#include "../basic.h"

int main(void)
{
	ucontext_t uc;
	volatile int times = 0;
	volatile double fp = 1.5;
	if ( getcontext(&uc) != 0 )
		err(1, "getcontext");
	times++;
	if ( times < 3 )
	{
		fp *= 2;
		setcontext(&uc);
		errx(1, "setcontext returned");
	}
	if ( times != 3 || fp != 6.0 )
		errx(1, "times %d, fp %g after two setcontext calls", times, fp);
	return 0;
}
