/*[SUSv4]*/
/* Test whether getmsg and getpmsg fail with EBADF for a descriptor not open for reading, else ENOSTR. */

#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int fds[2], flags = 0, band = 0;
	char buf[8];
	struct strbuf data = { 8, 0, buf };
	if ( pipe(fds) < 0 )
		err(1, "pipe");
	errno = 0;
	if ( getmsg(fds[0], NULL, &data, &flags) != -1 || errno != ENOSTR )
		errx(1, "getmsg on a pipe did not fail with ENOSTR");
	errno = 0;
	if ( getpmsg(fds[0], NULL, &data, &band, &flags) != -1 || errno != ENOSTR )
		errx(1, "getpmsg on a pipe did not fail with ENOSTR");
	errno = 0;
	if ( getmsg(fds[1], NULL, &data, &flags) != -1 || errno != EBADF )
		errx(1, "getmsg on a write end did not fail with EBADF");
	errno = 0;
	if ( getpmsg(-1, NULL, &data, &band, &flags) != -1 || errno != EBADF )
		errx(1, "getpmsg(-1) did not fail with EBADF");
	return 0;
}
