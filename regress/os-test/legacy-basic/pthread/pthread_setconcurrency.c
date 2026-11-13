/*[SUSv4]*/
/* Test whether pthread_getconcurrency gives back what pthread_setconcurrency was given. */

#include <errno.h>
#include <pthread.h>

#include "../basic.h"

int main(void)
{
	if ( pthread_getconcurrency() != 0 )
		errx(1, "pthread_getconcurrency before any set was not 0");
	if ( pthread_setconcurrency(4) != 0 )
		errx(1, "pthread_setconcurrency(4) failed");
	if ( pthread_getconcurrency() != 4 )
		errx(1, "pthread_getconcurrency did not give 4");
	if ( pthread_setconcurrency(-1) != EINVAL )
		errx(1, "pthread_setconcurrency(-1) was not EINVAL");
	pthread_setconcurrency(0);
	return 0;
}
