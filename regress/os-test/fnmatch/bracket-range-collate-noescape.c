/* Test whether brackets collate elements work in ranges combined with
   backslashes in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	// Control test that collation ranges are implemented.
	match("[[.b.]-[.d.]]", "c");

	// Backslashes are ordinary characters inside collation elements when in
	// noescape mode.
	reject("[[.\\.]-[.a.]]", "-");
	reject("[[.\\.]-[.a.]]", "[");
	match("[[.\\.]-[.a.]]", "]");
	match("[[.\\.]-[.a.]]", "\\");
	match("[[.\\.]-[.a.]]", "^");
	match("[[.\\.]-[.a.]]", "a");
	reject("[[.\\.]-[.a.]]", "b");

	reject("[[.[.]-[.\\.]]", "-");
	match("[[.[.]-[.\\.]]", "[");
	match("[[.[.]-[.\\.]]", "[");
	match("[[.[.]-[.\\.]]", "\\");
	reject("[[.[.]-[.\\.]]", "^");
	reject("[[.[.]-[.\\.]]", "a");

	reject("[[.\\.]-[.\\.]]", "-");
	reject("[[.\\.]-[.\\.]]", "[");
	reject("[[.\\.]-[.\\.]]", "[");
	match("[[.\\.]-[.\\.]]", "\\");
	reject("[[.\\.]-[.\\.]]", "^");
	reject("[[.\\.]-[.\\.]]", "a");

	return 0;
}
