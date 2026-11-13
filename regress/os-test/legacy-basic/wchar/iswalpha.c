/*[SUSv4]*/
/* Test whether <wchar.h> declares the classification functions. */

#include <wchar.h>

#include "../basic.h"

int main(void)
{
	if ( !iswalpha(L'a') || iswalpha(L'1') || !iswdigit(L'7') ||
	     towupper(L'a') != L'A' || !iswctype(L'x', wctype("lower")) )
		errx(1, "<wchar.h>'s classification functions gave a wrong answer");
	return 0;
}
