/*[SUSv2]*/
/* Test whether ttyslot is -1 without a terminal. */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	/* the descriptors 0, 1 and 2 without a terminal: -1 */
	int fd = open("/dev/null", O_RDWR);
	if ( fd < 0 )
		err(1, "/dev/null");
	int e = dup(2);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	int r = ttyslot();
	dup2(e, 2);
	if ( r != -1 )
		errx(1, "ttyslot without a terminal was %d", r);
	return 0;
}
