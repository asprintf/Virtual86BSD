/* Test whether FNM_CASEFOLD works with collation elements. */

#include "suite.h"

int main(void)
{
	// Control tests to verify case sensitivity works as a baseline.
	flags = 0;

	match("[[.a.]]", "a");
	reject("[[.a.]]", "A");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "B");

	match("[[.A.]-[.f.]]", "A");
	match("[[.A.]-[.f.]]", "a");
	match("[[.A.]-[.f.]]", "D");
	match("[[.A.]-[.f.]]", "d");
	match("[[.A.]-[.f.]]", "F");
	match("[[.A.]-[.f.]]", "f");
	match("[[.A.]-[.f.]]", "G");
	reject("[[.A.]-[.f.]]", "g");
	match("[[.A.]-[.f.]]", "Z");
	reject("[[.A.]-[.f.]]", "z");
	match("[[.A.]-[.f.]]", "^");
	reject("[[.A.]-[.f.]]", "|");

	reject("[![.a.]]", "a");
	match("[![.a.]]", "A");
	match("[![.a.]]", "b");
	match("[![.a.]]", "B");

	reject("[![.A.]-[.f.]]", "A");
	reject("[![.A.]-[.f.]]", "a");
	reject("[![.A.]-[.f.]]", "D");
	reject("[![.A.]-[.f.]]", "d");
	reject("[![.A.]-[.f.]]", "F");
	reject("[![.A.]-[.f.]]", "f");
	reject("[![.A.]-[.f.]]", "G");
	match("[![.A.]-[.f.]]", "g");
	reject("[![.A.]-[.f.]]", "Z");
	match("[![.A.]-[.f.]]", "z");
	reject("[![.A.]-[.f.]]", "^");
	match("[![.A.]-[.f.]]", "|");

	// Test that case insenstivity works.
	flags = FNM_CASEFOLD;

	match("[[.a.]]", "a");
	match("[[.a.]]", "A");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "B");

	match("[[.A.]-[.f.]]", "A");
	match("[[.A.]-[.f.]]", "a");
	match("[[.A.]-[.f.]]", "D");
	match("[[.A.]-[.f.]]", "d");
	match("[[.A.]-[.f.]]", "F");
	match("[[.A.]-[.f.]]", "f");
#if 1
	match("[[.A.]-[.f.]]", "G");
	match("[[.A.]-[.f.]]", "g");
	match("[[.A.]-[.f.]]", "Z");
	match("[[.A.]-[.f.]]", "z");
	match("[[.A.]-[.f.]]", "^");
#endif
	reject("[[.A.]-[.f.]]", "|");

	reject("[![.a.]]", "a");
	reject("[![.a.]]", "A");
	match("[![.a.]]", "b");
	match("[![.a.]]", "B");

	reject("[![.A.]-[.f.]]", "A");
	reject("[![.A.]-[.f.]]", "a");
	reject("[![.A.]-[.f.]]", "D");
	reject("[![.A.]-[.f.]]", "d");
	reject("[![.A.]-[.f.]]", "F");
	reject("[![.A.]-[.f.]]", "f");
#if 1
	reject("[![.A.]-[.f.]]", "G");
	reject("[![.A.]-[.f.]]", "g");
	reject("[![.A.]-[.f.]]", "Z");
	reject("[![.A.]-[.f.]]", "z");
	reject("[![.A.]-[.f.]]", "^");
#endif
	match("[![.A.]-[.f.]]", "|");

	return 0;
}
