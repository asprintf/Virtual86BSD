/* Test whether bracket equivalence class elements works. */

#include "suite.h"

void run(void)
{
	match("[[=a=]]", "a");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "A");

	partial("[[=a=][=b=]]", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("[[=a=][=b=]]", "ba", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("[[=a=][=b=]]", "cba", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });
	partial("[^[=a=][=b=]]", "cba", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });

	// [ is not a META_CHAR and there are no special rules for it, so it's just
	// a regular COLL_ELEM_SINGLE here, and it is allowed as an equivalence
	// class.
	reject("[[=[=]x]", "");
	reject("[[=[=]x]", ".");
	match("[[=[=]x]", "x");
	match("[[=[=]x]", "[");
	reject("[[=[=]x]", "]");
	reject("[[=[=]x]", "y");

	match("[[=\\=]]", "\\");
	reject("[[=\\=]]", "b");
	reject("[[=\\=]]", "=");
	reject("[[=\\=]]", "[");
	reject("[[=\\=]]", "]");

	match("[\\[=a=]]", "\\");
	match("[\\[=a=]]", "a");
	reject("[\\[=a=]]", "b");
	reject("[\\[=a=]]", "=");
	reject("[\\[=a=]]", "[");
	reject("[\\[=a=]]", "]");
	reject("[\\[=a=]]", "b]");
	reject("[\\[=a=]]", "=]");
	reject("[\\[=a=]]", "[]");
	reject("[\\[=a=]]", "]]");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
