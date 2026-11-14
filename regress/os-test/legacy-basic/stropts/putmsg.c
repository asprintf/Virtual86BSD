/*[SUSv4]*/
/* Test whether putmsg and putpmsg fail with EBADF for a descriptor not open for writing, else ENOSTR. */

#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	int fds[2];
	char buf[] = "x";
	struct strbuf data = { 1, 1, buf };
	if ( pipe(fds) < 0 )
		err(1, "pipe");
	errno = 0;
	if ( putmsg(fds[1], NULL, &data, 0) != -1 || errno != ENOSTR )
		errx(1, "putmsg on a pipe did not fail with ENOSTR");
	errno = 0;
	if ( putpmsg(fds[1], NULL, &data, 0, MSG_BAND) != -1 || errno != ENOSTR )
		errx(1, "putpmsg on a pipe did not fail with ENOSTR");
	errno = 0;
	if ( putmsg(fds[0], NULL, &data, 0) != -1 || errno != EBADF )
		errx(1, "putmsg on a read end did not fail with EBADF");
	errno = 0;
	if ( putpmsg(-1, NULL, &data, 0, 0) != -1 || errno != EBADF )
		errx(1, "putpmsg(-1) did not fail with EBADF");
	return 0;
}
