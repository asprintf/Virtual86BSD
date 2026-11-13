/* Test whether bracket collate elements works. */

#include "suite.h"

void run(void)
{
	match("[[.a.]]", "a");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "A");

	partial("[[.a.][.b.]]", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("[[.a.][.b.]]", "ba", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("[[.a.][.b.]]", "cba", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });
	partial("[^[.a.][.b.]]", "cba", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });

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

	match("[[.\\.]]", "\\");
	reject("[[.\\.]]", "b");
	reject("[[.\\.]]", ".");
	reject("[[.\\.]]", "[");
	reject("[[.\\.]]", "]");

	match("[\\[.a.]]", "\\");
	match("[\\[.a.]]", "a");
	reject("[\\[.a.]]", "b");
	reject("[\\[.a.]]", ".");
	reject("[\\[.a.]]", "[");
	reject("[\\[.a.]]", "]");
	reject("[\\[.a.]]", "b]");
	reject("[\\[.a.]]", ".]");
	reject("[\\[.a.]]", "[]");
	reject("[\\[.a.]]", "]]");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
