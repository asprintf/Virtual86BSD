/*[SUSv2]*/
/* Test whether regcmp and regex match and hand back ( ... )$n. */

#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "../basic.h"

int main(void)
{
	char* re = regcmp("([a-z]+)$0", "-", "([0-9]+)$1", (char*) 0);
	if ( !re )
		errx(1, "regcmp failed");
	const char* s = "  abc-123 rest";
	char r0[16], r1[16];
	char* end = regex(re, s, r0, r1);
	if ( !end )
		errx(1, "regex did not match");
	if ( end != s + 9 || __loc1 != s + 2 )
		errx(1, "regex: __loc1 at %d, end at %d", (int)(__loc1 - s), (int)(end - s));
	if ( strcmp(r0, "abc") != 0 || strcmp(r1, "123") != 0 )
		errx(1, "regex handed back \"%s\" and \"%s\"", r0, r1);
	free(re);
	if ( !(re = regcmp("x{2,3}y", (char*) 0)) )
		errx(1, "regcmp of x{2,3}y failed");
	if ( !regex(re, "axxxy") || regex(re, "axy") )
		errx(1, "x{2,3}y did not match as it should");
	free(re);
	if ( regcmp("(abc", (char*) 0) != NULL )
		errx(1, "regcmp of an unbalanced ( did not fail");
	return 0;
}
