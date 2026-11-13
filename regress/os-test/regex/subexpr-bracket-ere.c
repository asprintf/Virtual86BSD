/* Test whether ERE subexpressions work combined with brackets. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("([()])", "");
	reject("([()])", "x");
	matches("([()])", "(", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("([()])", ")", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("([()])", "()", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("([()])", ")(", 2, (M[]) { m(0, 1), m(0, 1) });

	reject("([(])([)])", "");
	reject("([(])([)])", ")");
	reject("([(])([)])", "(");
	matches("([(])([)])", "()", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	reject("([(])([)])", ")(");
	reject("([(])([)])", "(x)");

	return 0;
}
