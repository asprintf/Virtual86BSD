/*[nonstandard]*/
/* Test whether BRE asterisk is nonspecial after anchor in an alternation. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	reject("x\\|^*", "");
	reject("x\\|^*", "a");
	match("x\\|^*", "x");
	match("x\\|^*", "*");
	reject("x\\|^*", "aa");
	partial("x\\|^*", "ax", m(1, 2));
	reject("x\\|^*", "a*");
	partial("x\\|^*", "xa", m(0, 1));
	partial("x\\|^*", "xx", m(0, 1));
	partial("x\\|^*", "x*", m(0, 1));
	partial("x\\|^*", "*a", m(0, 1));
	partial("x\\|^*", "*x", m(0, 1));
	partial("x\\|^*", "**", m(0, 1));


	return 0;
}
