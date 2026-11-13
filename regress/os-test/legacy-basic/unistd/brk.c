/*[SUSv2]*/
/* Test whether sbrk and brk move the break and give zeroed space. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	char* old = sbrk(0);
	if ( old == (char*)-1 )
		err(1, "sbrk(0)");
	char* p = sbrk(8192);
	if ( p != old )
		errx(1, "sbrk(8192) did not return the prior break");
	if ( sbrk(0) != old + 8192 )
		errx(1, "the break did not move by 8192");
	for ( int i = 0; i < 8192; i++ )
		if ( p[i] != 0 )
			errx(1, "the new space is not zero at %d", i);
	memset(p, 0x55, 8192);
	if ( brk(old) != 0 )
		err(1, "brk");
	if ( sbrk(0) != old )
		errx(1, "brk did not set the break back");
	return 0;
}
