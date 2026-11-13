/* Test whether brackets collate elements works in ranges. */

#include "suite.h"

void run(void)
{
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

	reject("[^][.-.]-0]", "-");
	reject("[^][.-.]-0]", "/");
	reject("[^][.-.]-0]", "0");
	reject("[^][.-.]-0]", "]");
	match("[^][.-.]-0]", "1");
	match("[^][.-.]-0]", "[");
	match("[^][.-.]-0]", ",");

	reject("[[.[.]-[.].]]", "a");
	reject("[[.[.]-[.].]]", ".");
	reject("[[.[.]-[.].]]", "-");
	match("[[.[.]-[.].]]", "[");
	match("[[.[.]-[.].]]", "\\");
	match("[[.[.]-[.].]]", "]");

	reject("[[.\\.]-[.a.]]", "-");
	reject("[[.\\.]-[.a.]]", "[");
	match("[[.\\.]-[.a.]]", "]");
	match("[[.\\.]-[.a.]]", "\\");
	match("[[.\\.]-[.a.]]", "^");
	match("[[.\\.]-[.a.]]", "a");
	reject("[[.\\.]-[.a.]]", "b");

	reject("[[.[.]-[.\\.]]", "-");
	match("[[.[.]-[.\\.]]", "[");
	reject("[[.[.]-[.\\.]]", "]");
	match("[[.[.]-[.\\.]]", "\\");
	reject("[[.[.]-[.\\.]]", "^");
	reject("[[.[.]-[.\\.]]", "a");

	reject("[[.\\.]-[.\\.]]", "-");
	reject("[[.\\.]-[.\\.]]", "[");
	reject("[[.\\.]-[.\\.]]", "]");
	match("[[.\\.]-[.\\.]]", "\\");
	reject("[[.\\.]-[.\\.]]", "^");
	reject("[[.\\.]-[.\\.]]", "a");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
