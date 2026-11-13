/* Test whether bracket collation elements work with backslashes in noescape
   mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	// Control test that collation elements are implemented.
	match("[[.a.]]", "a");
	reject("[[.a.]]", "#");

	match("[\\[.a.]]", "\\");
	match("[\\[.a.]]", "a");
	reject("[\\[.a.]]", "b");
	reject("[\\[.a.]]", ".");
	reject("[\\[.a.]]", "[");
	reject("[\\[.a.]]", "]");
	reject("[\\[.a.]]", "a]");
	reject("[\\[.a.]]", "b]");
	reject("[\\[.a.]]", ".]");
	reject("[\\[.a.]]", "[]");
	reject("[\\[.a.]]", "]]");

	match("[[.\\.]]", "\\");
	reject("[[.\\.]]", "#");

	return 0;
}
