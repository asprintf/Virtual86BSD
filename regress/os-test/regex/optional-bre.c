/*[nonstandard]*/
/* Test whether BRE optionals work. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	reject("a\\?ab\\?", "");
	match("a\\?ab\\?", "a");
	reject("a\\?ab\\?", "b");
	partial("a\\?ab\\?", "aa", m(0, 2));
	partial("a\\?ab\\?", "ab", m(0, 2));
	partial("a\\?ab\\?", "ba", m(1, 2));
	reject("a\\?ab\\?", "bb");
	partial("a\\?ab\\?", "aaa", m(0, 2));
	partial("a\\?ab\\?", "aab", m(0, 3));
	partial("a\\?ab\\?", "aba", m(0, 2));
	partial("a\\?ab\\?", "abb", m(0, 2));
	partial("a\\?ab\\?", "baa", m(1, 3));
	partial("a\\?ab\\?", "bab", m(1, 3));
	partial("a\\?ab\\?", "bba", m(2, 3));
	reject("a\\?ab\\?", "bbb");

	return 0;
}
