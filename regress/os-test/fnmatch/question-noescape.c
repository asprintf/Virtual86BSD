/* Test whether question marks work with backslashes in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	reject("\\?", "");
	reject("\\?", "?");
	match("\\?", "\\?");
	reject("\\?", "a");
	reject("\\?", "aaaa");
	reject("\\?", "abc");
	match("\\?", "\\a");
	reject("\\?", "\\aaa");
	reject("\\?", "\\abc");

	return 0;
}
