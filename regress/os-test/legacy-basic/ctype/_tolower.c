/*[SUSv4]*/
/* Test whether _tolower converts an uppercase letter. */

#include <ctype.h>

#include "../basic.h"

int main(void)
{
	if ( _tolower('A') != 'a' || _tolower('Z') != 'z' )
		errx(1, "_tolower did not convert an uppercase letter");
#undef _tolower
	if ( _tolower('Q') != 'q' )
		errx(1, "the function _tolower did not convert an uppercase letter");
	return 0;
}
