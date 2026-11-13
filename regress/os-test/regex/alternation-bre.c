/*[nonstandard]*/
/* Test whether BRE alternations work. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	reject("a\\|b", "");
	match("a\\|b", "a");
	match("a\\|b", "b");
	reject("a\\|b", "c");
	reject("a\\|b", "|");
	reject("a\\|b", "\\");
	partial("a\\|b", "caa", m(1, 2));
	partial("a\\|b", "cab", m(1, 2));
	partial("a\\|b", "cba", m(1, 2));
	partial("a\\|b", "cbb", m(1, 2));
	partial("a\\|b\\|c", "cbb", m(0, 1));

	reject("ab\\|cc", "");
	reject("ab\\|cc", "a");
	reject("ab\\|cc", "b");
	reject("ab\\|cc", "c");
	reject("ab\\|cc", "aa");
	reject("ab\\|cc", "ba");
	reject("ab\\|cc", "ca");
	match("ab\\|cc", "ab");
	reject("ab\\|cc", "bb");
	reject("ab\\|cc", "cb");
	reject("ab\\|cc", "ac");
	reject("ab\\|cc", "bc");
	match("ab\\|cc", "cc");

	reject("ab\\|cc", "aaa");
	partial("ab\\|cc", "aba", m(0, 2));
	reject("ab\\|cc", "aca");
	partial("ab\\|cc", "aab", m(1, 3));
	partial("ab\\|cc", "abb", m(0, 2));
	reject("ab\\|cc", "acb");
	reject("ab\\|cc", "aac");
	partial("ab\\|cc", "abc", m(0, 2));
	partial("ab\\|cc", "acc", m(1, 3));

	return 0;
}
