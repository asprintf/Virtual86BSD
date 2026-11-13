/*[SUSv4]*/
/* Test whether toascii returns c & 0x7f. */

#include <ctype.h>

#include "../basic.h"

int main(void)
{
	if ( toascii(0x1C1) != 0x41 )
		errx(1, "toascii(0x1C1) was %#x, not 0x41", toascii(0x1C1));
	if ( toascii('z') != 'z' )
		errx(1, "toascii changed an ASCII character");
	return 0;
}
