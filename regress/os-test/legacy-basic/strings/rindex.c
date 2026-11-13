/*[SUSv3]*/
/* Test whether rindex is strrchr. */

#include <strings.h>

#include "../basic.h"

int main(void)
{
	const char* s = "abcabc";
	if ( rindex(s, 'b') != s + 4 )
		errx(1, "rindex did not find the last b");
	if ( rindex(s, 'x') != NULL )
		errx(1, "rindex found a byte that is not there");
	if ( rindex(s, '\0') != s + 6 )
		errx(1, "rindex did not find the terminating null byte");
	return 0;
}
