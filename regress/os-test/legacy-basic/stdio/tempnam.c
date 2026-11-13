/*[SUSv4]*/
/* Test whether tempnam names a new file in the directory with the prefix. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../basic.h"

int main(void)
{
	struct stat st;
	/* five bytes of the prefix; @ is not in the portable filename set */
	char* p = tempnam("/tmp", "zz@@@@@");
	if ( !p )
		err(1, "tempnam");
	if ( strncmp(p, "/tmp/zz@@@", 10) != 0 || strncmp(p, "/tmp/zz@@@@", 11) == 0 )
		errx(1, "tempnam(\"/tmp\", \"zz@@@@@\") gave %s", p);
	if ( lstat(p, &st) == 0 || errno != ENOENT )
		errx(1, "tempnam's name %s exists", p);
	free(p);
	/* not a directory: P_tmpdir (or an implementation-defined one) */
	if ( !(p = tempnam("/nonexistent/dir", NULL)) )
		err(1, "tempnam");
	if ( strncmp(p, P_tmpdir, strlen(P_tmpdir)) != 0 && strncmp(p, "/tmp/", 5) != 0 )
		errx(1, "tempnam with no such directory gave %s", p);
	free(p);
	return 0;
}
