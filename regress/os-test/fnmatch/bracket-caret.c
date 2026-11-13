/*[nonstandard]*/
/* Test whether bracket negation work with ^ as a nonstandard extension. */

#include "suite.h"

int main(void)
{
	// Most systems allow ^ in addition to ! these days.
	reject("[^x]", "x");
	match("[^x]", "^");
	match("[^x]", "y");
	match("[^x]", "!");

	return 0;
}
