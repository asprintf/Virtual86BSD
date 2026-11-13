/*[nonstandard]*/
/* Test whether BRE asterisk is nonspecial after anchor in a subexpression. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	matches("\\(^*\\)", "*", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("\\(^*\\)", "x");
	reject("\\(^*\\)", "a*");

	return 0;
}
