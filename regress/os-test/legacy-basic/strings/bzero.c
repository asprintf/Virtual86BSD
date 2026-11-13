/*[SUSv3]*/
/* Test whether bzero zeroes n bytes and no more. */

#include <strings.h>

#include "../basic.h"

int main(void)
{
	char buf[8] = "abcdefg";
	bzero(buf + 1, 5);
	for ( int i = 1; i < 6; i++ )
		if ( buf[i] != 0 )
			errx(1, "bzero left byte %d as %d", i, buf[i]);
	if ( buf[0] != 'a' || buf[6] != 'g' )
		errx(1, "bzero changed a byte outside the area");
	return 0;
}
