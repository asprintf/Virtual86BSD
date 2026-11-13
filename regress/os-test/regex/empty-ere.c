/*[nonstandard]*/
/* Test whether ERE empty pattern works. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;
	partial("", "", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "a", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "b", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "abc", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	eflags = REG_NOTBOL | REG_NOTEOL;
	partial("", "", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "D", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "E", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	partial("", "DEF", (regmatch_t) { .rm_so = 0, .rm_eo = 0 });
	return 0;
}
