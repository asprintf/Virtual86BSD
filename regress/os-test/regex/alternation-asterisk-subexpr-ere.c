/* Test whether ERE alternations combine with subexpressions and asterisk. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	// POSIX.1-2024 regcomp:
	// "If subexpression i is contained within another subexpression j, and i
	//  is not contained within any other subexpression that is contained within
	//  j, and a match of subexpression j is reported in pmatch[j], then the
	//  match or non-match of subexpression i reported in pmatch[i] shall be as
	//  described in 1. and 2. above, but within the substring reported in
	//  pmatch[j] rather than the whole string. The offsets in pmatch[i] are
	//  still relative to the start of string."
	// glibc fails on this test because it remembers the match of a, which does
	// not appear within the match of b, which is the last overall match of the
	// outermost subexpression.
	matches("((a)|(b))*", "ab", 4, (M[]) { m(0, 2), m(1, 2), m(-1, -1), m(1, 2) });

	return 0;
}
