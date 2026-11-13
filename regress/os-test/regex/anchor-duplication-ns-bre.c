/*[nonstandard]*/
/* Test whether BRE anchors work combined with duplications (nonstandard). */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("^a\\?$", "");
	match("^a\\?$", "a");
	reject("^a\\?$", "b");
	reject("^a\\?$", "aa");
	reject("^a\\?$", "ab");
	reject("^a\\?$", "ba");
	reject("^a\\?$", "bb");

	reject("^a\\+$", "");
	match("^a\\+$", "a");
	reject("^a\\+$", "b");
	match("^a\\+$", "aa");
	reject("^a\\+$", "ab");
	reject("^a\\+$", "ba");
	reject("^a\\+$", "bb");

	reject("^a\\{2\\}$", "");
	reject("^a\\{2\\}$", "a");
	reject("^a\\{2\\}$", "b");
	match("^a\\{2\\}$", "aa");
	reject("^a\\{2\\}$", "ab");
	reject("^a\\{2\\}$", "ba");
	reject("^a\\{2\\}$", "bb");

	matches("\\(^\\)\\{2\\}", "", 2, (M[]) { m(0, 0), m(0, 0) });
	matches("\\(^\\)\\{2\\}", "x", 2, (M[]) { m(0, 0), m(0, 0) });
	matches("\\($\\)\\{2\\}", "", 2, (M[]) { m(0, 0), m(0, 0) });
	matches("\\($\\)\\{2\\}", "x", 2, (M[]) { m(1, 1), m(1, 1) });

	return 0;
}
