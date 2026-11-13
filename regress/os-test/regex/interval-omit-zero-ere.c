/*[nonstandard]*/
/* Test whether ERE intervals can omit the lower bound with 0 upper bound. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	// POSIX only defines the syntaxes "{m}", "{m,}", or "{m,n}", but some
	// implementations allow omitting m as an implicit 0. However, many systems
	// fail on a zero upper bound.
	match("a{,0}", "");
	partial("a{,0}", "a", m(0, 0));
	partial("a{,0}", "aa", m(0, 0));

	return 0;
}
