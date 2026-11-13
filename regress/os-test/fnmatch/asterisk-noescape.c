/* Test whether asterisks work with backslashes in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	reject("\\*", "");
	reject("\\*", "*");
	match("\\*", "\\*");
	reject("\\*", "a");
	reject("\\*", "aaaa");
	reject("\\*", "abc");
	match("\\*", "\\a");
	match("\\*", "\\aaa");
	match("\\*", "\\abc");

	return 0;
}
