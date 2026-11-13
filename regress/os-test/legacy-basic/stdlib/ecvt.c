/*[SUSv3]*/
/* Test whether ecvt gives ndigit digits, the radix position and the sign. */

#include <stdlib.h>
#include <string.h>

#include "../basic.h"

static void check(double v, int n, const char* digits, int decpt, int sign)
{
	int d, s;
	char* r = ecvt(v, n, &d, &s);
	if ( strcmp(r, digits) != 0 || d != decpt || !s != !sign )
		errx(1, "ecvt(%g, %d) gave \"%s\" %d %d, not \"%s\" %d %d",
		     v, n, r, d, s, digits, decpt, sign);
}

int main(void)
{
	check(1234.5678, 6, "123457", 4, 0);
	/* the rounding is implementation-defined: no value near a tie */
	check(-0.0127, 2, "13", -1, 1);
	check(0.5, 3, "500", 0, 0);
	/* rounding that carries: still ndigit digits */
	check(9.96, 2, "10", 2, 0);
	/* the high-order digit is non-zero */
	check(0.000123, 3, "123", -3, 0);
	int d, s;
	char* r = ecvt(0.0, 3, &d, &s);
	if ( strcmp(r, "000") != 0 || (d != 0 && d != 1) )
		errx(1, "ecvt(0, 3) gave \"%s\" %d", r, d);
	return 0;
}
