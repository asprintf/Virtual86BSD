/* Test whether BRE anchors work combined with duplications. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("^a*$", "");
	match("^a*$", "a");
	reject("^a*$", "b");
	match("^a*$", "aa");
	reject("^a*$", "ab");
	reject("^a*$", "ba");
	reject("^a*$", "bb");

	return 0;
}
