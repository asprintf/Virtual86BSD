/* Test whether BRE period characters works. */

#include "suite.h"

int main(void)
{
	cflags = 0;
	match(".", "a");
	match(".", "b");
	reject(".", "");
	partial(".", "abc", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("..", "abc", (regmatch_t) { .rm_so = 0, .rm_eo = 2 });
	match("...", "abc");
	match(".", " ");
	match(".", "\n");
	match(".", "/");
	eflags = REG_NOTBOL | REG_NOTEOL;
	match(".", "D");
	match(".", "E");
	reject(".", "");
	partial(".", "DEF", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("..", "DEF", (regmatch_t) { .rm_so = 0, .rm_eo = 2 });
	match("...", "DEF");
	match(".", " ");
	match(".", "\n");
	match(".", "/");
	return 0;
}
