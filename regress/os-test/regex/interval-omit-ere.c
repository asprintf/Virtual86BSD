/*[nonstandard]*/
/* Test whether ERE intervals can omit the lower bound. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	// POSIX only defines the syntaxes "{m}", "{m,}", or "{m,n}", but some
	// implementations allow omitting m as an implicit 0.
	match("a{,3}", "");
	match("a{,3}", "a");
	match("a{,3}", "aa");
	match("a{,3}", "aaa");
	partial("a{,3}", "aaaa", m(0, 3));
	partial("a{,3}", "aaaaa", m(0, 3));

	return 0;
}
