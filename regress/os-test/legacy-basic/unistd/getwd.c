/*[SUSv3]*/
/* Test whether getwd gives the working directory. */

#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	char a[PATH_MAX + 1], b[PATH_MAX + 1];
	if ( chdir("/tmp") < 0 )
		err(1, "/tmp");
	if ( !getcwd(a, sizeof a) )
		err(1, "getcwd");
	if ( getwd(b) != b )
		errx(1, "getwd did not return its argument");
	if ( strcmp(a, b) != 0 )
		errx(1, "getwd gave %s, getcwd %s", b, a);
	return 0;
}
