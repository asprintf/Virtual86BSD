/*[nonstandard]*/
/* Test whether BRE empty pattern works. */

#include "suite.h"

int main(void)
{
	cflags = 0;
	partial("", "", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "a", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "b", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "abc", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	eflags = REG_NOTBOL | REG_NOTEOL;
	partial("", "D", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "E", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "DEF", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	return 0;
}
