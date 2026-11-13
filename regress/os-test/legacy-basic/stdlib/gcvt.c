/*[SUSv3]*/
/* Test whether gcvt converts as %g with ndigit significant digits. */

#include <stdlib.h>
#include <string.h>

#include "../basic.h"

static void check(double v, int n, const char* want)
{
	char buf[64];
	char* r = gcvt(v, n, buf);
	if ( r != buf )
		errx(1, "gcvt did not return buf");
	if ( strcmp(buf, want) != 0 )
		errx(1, "gcvt(%g, %d) gave \"%s\", not \"%s\"", v, n, buf, want);
}

int main(void)
{
	check(123.456, 5, "123.46");
	check(-0.000123456, 3, "-0.000123");
	check(100.0, 3, "100");
	check(1.5e10, 3, "1.5e+10");
	check(2.50, 6, "2.5");
	return 0;
}
