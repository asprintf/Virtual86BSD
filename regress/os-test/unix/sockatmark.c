/* Test sockatmark on a freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	int ret;
	if ( (ret = sockatmark(fd)) < 0 )
		err(1, "sockatmark");
	// sockatmark is supposed to return 0 because the socket is not at an
	// out-of-band mark.
	printf("%d\n", ret);
	return 0;
}
