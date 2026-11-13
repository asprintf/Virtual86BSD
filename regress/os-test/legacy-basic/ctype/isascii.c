/*[SUSv4]*/
/* Test whether isascii accepts 0 to 0177 and nothing else. */

#include <ctype.h>

#include "../basic.h"

int main(void)
{
	if ( !isascii(0) || !isascii('A') || !isascii(0177) )
		errx(1, "isascii rejected a 7-bit character");
	if ( isascii(0200) || isascii(-1) || isascii(1000) )
		errx(1, "isascii accepted a value outside 0 to 0177");
	return 0;
}
