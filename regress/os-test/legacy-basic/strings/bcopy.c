/*[SUSv3]*/
/* Test whether bcopy copies, also between overlapping areas. */

#include <string.h>
#include <strings.h>

#include "../basic.h"

int main(void)
{
	char buf[16] = "0123456789";
	char dst[16];
	bcopy("hello", dst, 6);
	if ( strcmp(dst, "hello") != 0 )
		errx(1, "bcopy did not copy: %s", dst);
	bcopy(buf, buf + 2, 8);	/* overlapping, to a higher address */
	if ( memcmp(buf, "0101234567", 10) != 0 )
		errx(1, "bcopy upwards gave %.10s", buf);
	strcpy(buf, "0123456789");
	bcopy(buf + 2, buf, 8);	/* overlapping, to a lower address */
	if ( memcmp(buf, "2345678989", 10) != 0 )
		errx(1, "bcopy downwards gave %.10s", buf);
	return 0;
}
