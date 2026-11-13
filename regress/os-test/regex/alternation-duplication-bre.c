/*[nonstandard]*/
/* Test whether BRE alternations work combined with duplications. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "");
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "a", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "b", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "c", m(0, 0));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "d", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "aa", m(0, 2));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "ab", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "ac", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "ad", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "ba", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "bb", m(0, 2));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "bc", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "bd", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "ca", m(0, 0));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "cb", m(0, 0));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "cc", m(0, 2));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "cd", m(0, 0));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "da", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "db", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "dc", m(0, 1));
	partial("a*\\|b\\+\\|c\\{2\\}\\|d\\?", "dd", m(0, 1));

	return 0;
}
