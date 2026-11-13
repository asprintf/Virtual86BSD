/* Test whether brackets collate elements work in ranges. */

#include "suite.h"

int main(void)
{
	// Collation ranges have to be implemented.
	reject("[[.b.]-[.d.]]", "a");
	match("[[.b.]-[.d.]]", "b");
	match("[[.b.]-[.d.]]", "c");
	match("[[.b.]-[.d.]]", "d");
	reject("[[.b.]-[.d.]]", "e");

	reject("[b-[.d.]]", "a");
	match("[b-[.d.]]", "b");
	match("[b-[.d.]]", "c");
	match("[b-[.d.]]", "d");
	reject("[b-[.d.]]", "e");

	reject("[[.b.]-d]", "a");
	match("[[.b.]-d]", "b");
	match("[[.b.]-d]", "c");
	match("[[.b.]-d]", "d");
	reject("[[.b.]-d]", "e");

	reject("[!][.-.]-0]", "-");
	reject("[!][.-.]-0]", "/");
	reject("[!][.-.]-0]", "0");
	reject("[!][.-.]-0]", "]");
	match("[!][.-.]-0]", "1");
	match("[!][.-.]-0]", "[");
	match("[!][.-.]-0]", ",");

	reject("[[.].]-[.a.]]", "-");
	reject("[[.].]-[.a.]]", "[");
	match("[[.].]-[.a.]]", "]");
	reject("[[.].]-[.a.]]", "\\");
	match("[[.].]-[.a.]]", "^");
	match("[[.].]-[.a.]]", "a");
	reject("[[.].]-[.a.]]", "b");

	reject("[[.[.]-[.].]]", "-");
	match("[[.[.]-[.].]]", "[");
	match("[[.[.]-[.].]]", "]");
	match("[[.[.]-[.].]]", "\\");
	reject("[[.[.]-[.].]]", "^");
	reject("[[.[.]-[.].]]", "a");

	reject("[[.].]-[.].]]", "-");
	reject("[[.].]-[.].]]", "[");
	match("[[.].]-[.].]]", "]");
	reject("[[.].]-[.].]]", "\\");
	reject("[[.].]-[.].]]", "^");
	reject("[[.].]-[.].]]", "a");

	return 0;
}
