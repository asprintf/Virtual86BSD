/* Test whether BRE anchors work combined with subexpressions. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	reject("^\\(x\\(y\\)\\)", "");
	reject("^\\(x\\(y\\)\\)", "x");
	reject("^\\(x\\(y\\)\\)", "y");
	matches("^\\(x\\(y\\)\\)", "xy", 3, (M[]) { m(0, 2), m(0, 2), m(1, 2) });
	matches("^\\(x\\(y\\)\\)", "xyz", 3, (M[]) { m(0, 2), m(0, 2), m(1, 2) });
	reject("^\\(x\\(y\\)\\)", "zxy");

	reject("\\(x\\(y\\)\\)$", "");
	reject("\\(x\\(y\\)\\)$", "x");
	reject("\\(x\\(y\\)\\)$", "y");
	matches("\\(x\\(y\\)\\)$", "xy", 3, (M[]) { m(0, 2), m(0, 2), m(1, 2) });
	reject("\\(x\\(y\\)\\)$", "xyz");
	matches("\\(x\\(y\\)\\)$", "zxy", 3, (M[]) { m(1, 3), m(1, 3), m(2, 3) });

	reject("^\\(x\\(y\\)\\)$", "");
	reject("^\\(x\\(y\\)\\)$", "x");
	reject("^\\(x\\(y\\)\\)$", "y");
	matches("^\\(x\\(y\\)\\)$", "xy", 3, (M[]) { m(0, 2), m(0, 2), m(1, 2) });
	reject("^\\(x\\(y\\)\\)$", "xyz");
	reject("^\\(x\\(y\\)\\)$", "zxy");

	return 0;
}
