/* Test whether BRE anchors work combined with brackets. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	reject("^[ab][a]$", "");
	reject("^[ab][a]$", "a");
	reject("^[ab][a]$", "b");
	match("^[ab][a]$", "aa");
	reject("^[ab][a]$", "ab");
	match("^[ab][a]$", "ba");
	reject("^[ab][a]$", "bb");
	reject("^[ab][a]$", "aaa");
	reject("^[ab][a]$", "aab");
	reject("^[ab][a]$", "aba");
	reject("^[ab][a]$", "abb");
	reject("^[ab][a]$", "baa");
	reject("^[ab][a]$", "bab");
	reject("^[ab][a]$", "bba");
	reject("^[ab][a]$", "bbb");

	return 0;
}
