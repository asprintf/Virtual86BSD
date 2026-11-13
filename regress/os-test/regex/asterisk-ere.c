/* Test whether ERE asterisk characters works. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	match("a*", "");
	match("a*", "a");
	match("a*", "aa");
	match("a*", "aaa");
	partial("a*", "b", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("a*", "ba", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("a*", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("a*", "aab", (regmatch_t) { .rm_so = 0, .rm_eo = 2 });

	return 0;
}
