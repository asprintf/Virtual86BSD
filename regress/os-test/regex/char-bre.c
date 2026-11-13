/* Test whether BRE single characters work. */

#include "suite.h"

int main(void)
{
	cflags = 0;
	match("c", "c");
	reject("c", "");
	reject("c", "d");
	reject("c", "C");
	match("ab", "ab");
	reject("ab", "ac");
	reject("ab", "cb");
	reject("ab", "cc");
	match("abc", "abc");
	partial("a", "abc", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("b", "abc", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });
	partial("c", "abc", (regmatch_t) { .rm_so = 2, .rm_eo = 3 });
	partial("bc", "abc", (regmatch_t) { .rm_so = 1, .rm_eo = 3 });
	eflags = REG_NOTBOL | REG_NOTEOL;
	match("d", "d");
	match("def", "def");
	reject("de", "df");
	match("]", "]");
	reject("]", "x");
	match("(", "(");
	reject("(", "x");
	match(")", ")");
	reject(")", "x");
	match("|", "|");
	reject("|", "x");
	match("{", "{");
	reject("{", "x");
	match("}", "}");
	reject("}", "x");
	match("+", "+");
	reject("+", "x");
	match("?", "?");
	reject("?", "x");
	match("*", "*");
	reject("*", "x");
	match("a]", "a]");
	reject("a]", "ax");
	match("a(", "a(");
	reject("a(", "ax");
	match("a)", "a)");
	reject("a)", "ax");
	match("a|", "a|");
	reject("a|", "ax");
	match("a{", "a{");
	reject("a{", "ax");
	match("a}", "a}");
	reject("a}", "ax");
	match("a+", "a+");
	reject("a+", "ax");
	match("a?", "a?");
	reject("a?", "ax");
	return 0;
}
