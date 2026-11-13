/* Test whether brackets collate elements work in ranges combined with
   backslashes. */

#include "suite.h"

int main(void)
{
	flags = 0;

	// Control test that collation ranges are implemented.
	match("[[.b.]-[.d.]]", "c");

	// Like discussed in bracket-collate-escape.c, escapes have to work inside
	// bracket expressions, and even collation elements.
	reject("[[.\\\\.]-[.a.]]", "-");
	reject("[[.\\\\.]-[.a.]]", "[");
	match("[[.\\\\.]-[.a.]]", "]");
	match("[[.\\\\.]-[.a.]]", "\\");
	match("[[.\\\\.]-[.a.]]", "^");
	match("[[.\\\\.]-[.a.]]", "a");
	reject("[[.\\\\.]-[.a.]]", "b");

	reject("[[.[.]-[.\\\\.]]", "-");
	match("[[.[.]-[.\\\\.]]", "[");
	reject("[[.[.]-[.\\\\.]]", "]");
	match("[[.[.]-[.\\\\.]]", "\\");
	reject("[[.[.]-[.\\\\.]]", "^");
	reject("[[.[.]-[.\\\\.]]", "a");

	reject("[[.\\\\.]-[.\\\\.]]", "-");
	reject("[[.\\\\.]-[.\\\\.]]", "[");
	reject("[[.\\\\.]-[.\\\\.]]", "]");
	match("[[.\\\\.]-[.\\\\.]]", "\\");
	reject("[[.\\\\.]-[.\\\\.]]", "^");
	reject("[[.\\\\.]-[.\\\\.]]", "a");

	return 0;
}
