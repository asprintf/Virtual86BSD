/* Test whether ERE asterisks work combined with subexpressions and optional. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	matches("(a?)*", "", 2, (M[]) { m(0, 0), m(0, 0) });
	matches("(a?)*", "a", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("(a?)*", "aa", 2, (M[]) { m(0, 2), m(1, 2) });
	matches("(a?)*", "aaa", 2, (M[]) { m(0, 3), m(2, 3) });

	return 0;
}
