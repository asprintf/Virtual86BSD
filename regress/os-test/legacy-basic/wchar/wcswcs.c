/*[SUSv3]*/
/* Test whether wcswcs finds a wide-character substring. */

#include <wchar.h>

#include "../basic.h"

int main(void)
{
	const wchar_t* s = L"hello world";
	if ( wcswcs(s, L"wor") != s + 6 )
		errx(1, "wcswcs did not find the substring");
	if ( wcswcs(s, L"xyz") != NULL )
		errx(1, "wcswcs found a substring that is not there");
	if ( wcswcs(s, L"") != s )
		errx(1, "wcswcs of the empty string was not the string");
	return 0;
}
