/* Test whether ERE plus characters works. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("a+", "");
	match("a+", "a");
	match("a+", "aa");
	match("a+", "aaa");
	reject("a+", "b");
	partial("a+", "ba", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });
	partial("a+", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("a+", "aab", (regmatch_t) { .rm_so = 0, .rm_eo = 2 });

	return 0;
}
