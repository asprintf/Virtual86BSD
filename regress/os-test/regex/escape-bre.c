/* Test whether BRE escape sequences work. */

#include "suite.h"

int main(void)
{
	cflags = 0;
	reject("\\\\", "");
	match("\\\\", "\\");
	reject("\\\\", "x");
	partial("\\\\", "\\\\", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	match("\\[", "[");
	reject("\\[", "x");
	match("\\]", "]");
	reject("\\]", "x");
	match("\\.", ".");
	reject("\\.", "x");
	match("\\*", "*");
	reject("\\*", "x");
	match("\\^", "^");
	reject("\\^", "x");
	match("\\$", "$");
	reject("\\$", "x");
	return 0;
}
