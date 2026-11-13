/* Test whether bracket collation elements work. */

#include "suite.h"

int main(void)
{
	// Collation elements have to be implemented.
	match("[[.a.]]", "a");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "A");
	reject("[[.a.]]", "c");

	match("[[.a.][.b.]]", "a");
	match("[[.a.][.b.]]", "b");
	reject("[[.a.][.b.]]", "A");
	reject("[[.a.][.b.]]", "c");

	reject("[![.a.][.b.]]", "a");
	reject("[![.a.][.b.]]", "b");
	match("[![.a.][.b.]]", "A");
	match("[![.a.][.b.]]", "c");

	reject("[[.].]x]", "");
	reject("[[.].]x]", ".");
	match("[[.].]x]", "x");
	reject("[.].]x]", "[");
	match("[[.].]x]", "]");
	reject("[[.].]x]", "y");

	match("[[.\n.]]", "\n");
	reject("[[.\n.]]", " ");
	reject("[[.\n.]]", "x");

	match("[[...]]", ".");
	reject("[[...]]", "a");

	return 0;
}
