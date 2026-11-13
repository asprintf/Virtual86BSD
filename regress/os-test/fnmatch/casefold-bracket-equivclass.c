/* Test whether FNM_CASEFOLD works with equivalence classes. */

#include "suite.h"

int main(void)
{
	// Control tests to verify case sensitivity works as a baseline.
	flags = 0;

	match("[[=a=]]", "a");
	reject("[[=a=]]", "A");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "B");

	reject("[![=a=]]", "a");
	match("[![=a=]]", "A");
	match("[![=a=]]", "b");
	match("[![=a=]]", "B");

	// Test that case insenstivity works.
	flags = FNM_CASEFOLD;

	match("[[=a=]]", "a");
	match("[[=a=]]", "A");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "B");

	reject("[![=a=]]", "a");
	reject("[![=a=]]", "A");
	match("[![=a=]]", "b");
	match("[![=a=]]", "B");

	return 0;
}
