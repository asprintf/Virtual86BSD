/*[SUSv3]*/
/* Test whether bcmp compares as SUSv3 says. */

#include <strings.h>

#include "../basic.h"

int main(void)
{
	if ( bcmp("abcd", "abcd", 4) != 0 )
		errx(1, "bcmp of equal areas was not 0");
	if ( bcmp("abcd", "abce", 4) == 0 )
		errx(1, "bcmp of different areas was 0");
	if ( bcmp("abcd", "abce", 3) != 0 )
		errx(1, "bcmp of equal first 3 bytes was not 0");
	if ( bcmp("a", "b", 0) != 0 )
		errx(1, "bcmp with n 0 was not 0");
	return 0;
}
