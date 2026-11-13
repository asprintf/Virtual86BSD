/*[SUSv3]*/
/* Test whether mktemp makes a name that no file has. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../basic.h"

int main(void)
{
	char template[] = "/tmp/lb.mktemp.XXXXXX";
	char* r = mktemp(template);
	if ( r != template )
		errx(1, "mktemp did not return template");
	if ( !template[0] )
		errx(1, "mktemp could not make a name");
	if ( strncmp(template, "/tmp/lb.mktemp.", 15) != 0 ||
	     strlen(template) != 21 || !strcmp(template + 15, "XXXXXX") )
		errx(1, "mktemp made %s", template);
	struct stat st;
	if ( lstat(template, &st) == 0 || errno != ENOENT )
		errx(1, "mktemp's name %s exists", template);
	char bad[] = "/tmp/lb.mktemp";
	mktemp(bad);
	if ( bad[0] != '\0' )
		errx(1, "mktemp without six X's did not give the null string");
	return 0;
}
