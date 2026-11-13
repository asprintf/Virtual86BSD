/* Test whether BRE asterisk characters works. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("a*", "");
	match("a*", "a");
	match("a*", "aa");
	match("a*", "aaa");
	partial("a*", "b", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("a*", "ba", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("a*", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("a*", "aab", (regmatch_t) { .rm_so = 0, .rm_eo = 2 });
	match("*", "*");
	reject("*", "");
	reject("*", "a");
	match("^*", "*");
	reject("^*", "");
	reject("^*", "a");
	reject("x|*", "");

	return 0;
}
