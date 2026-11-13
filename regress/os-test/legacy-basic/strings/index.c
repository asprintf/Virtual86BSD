/*[SUSv3]*/
/* Test whether index is strchr. */

#include <strings.h>

#include "../basic.h"

int main(void)
{
	const char* s = "abcabc";
	if ( index(s, 'b') != s + 1 )
		errx(1, "index did not find the first b");
	if ( index(s, 'x') != NULL )
		errx(1, "index found a byte that is not there");
	if ( index(s, '\0') != s + 6 )
		errx(1, "index did not find the terminating null byte");
	return 0;
}
