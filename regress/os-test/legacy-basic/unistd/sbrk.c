/*[SUSv2]*/
/* Test whether sbrk fails with ENOMEM for an impossible size. */

#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	errno = 0;
	if ( sbrk(INTPTR_MAX / 2) != (void*)-1 || errno != ENOMEM )
		errx(1, "sbrk of a huge size did not fail with ENOMEM");
	return 0;
}
