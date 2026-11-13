/*[SUSv3]*/
/* Test whether fcvt gives ndigit digits after the radix character. */

#include <stdlib.h>
#include <string.h>

#include "../basic.h"

static void check(double v, int n, const char* digits, int decpt, int sign)
{
	int d, s;
	char* r = fcvt(v, n, &d, &s);
	if ( strcmp(r, digits) != 0 || d != decpt || !s != !sign )
		errx(1, "fcvt(%g, %d) gave \"%s\" %d %d, not \"%s\" %d %d",
		     v, n, r, d, s, digits, decpt, sign);
}

int main(void)
{
	check(3.14159, 2, "314", 1, 0);
	check(-123.456, 1, "1235", 3, 1);
	check(0.5, 3, "500", 0, 0);
	check(1234.0, 0, "1234", 4, 0);
	check(0.0012, 4, "12", -2, 0);
	return 0;
}
