/*[SUSv4]*/
/* Test whether _toupper converts a lowercase letter. */

#include <ctype.h>

#include "../basic.h"

int main(void)
{
	if ( _toupper('a') != 'A' || _toupper('z') != 'Z' )
		errx(1, "_toupper did not convert a lowercase letter");
#undef _toupper
	if ( _toupper('q') != 'Q' )
		errx(1, "the function _toupper did not convert a lowercase letter");
	return 0;
}
