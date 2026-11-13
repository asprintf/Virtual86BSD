/* Test whether ERE anchors work. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	match("^", "");
	partial("^", "x", m(0, 0));
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "xy", m(0, 1));
	reject("^x", "yx");
	partial("x", "yx", m(1, 2));

	match("$", "");
	partial("$", "x", m(1, 1));
	match("x$", "x");
	reject("x$", "y");
	partial("x$", "yx", m(1, 2));
	reject("x$", "xy");
	partial("x", "xy", m(0, 1));
	match("^$", "");
	reject("^$", "x");

	match("^x$", "x");
	reject("^x$", "y");
	match("^xy$", "xy");
	reject("^xy$", "yx");
	reject("^x$", "yx");

	reject("\\^", "");
	match("\\^", "^");
	reject("\\^", "x");

	reject("\\$", "");
	match("\\$", "$");
	reject("\\$", "x");

	reject("\\^x", "");
	match("\\^x", "^x");
	reject("\\^x", "^");
	reject("\\^x", "x");
	reject("\\^x", "x^");

	reject("x\\$", "");
	match("x\\$", "x$");
	reject("x\\$", "$");
	reject("x\\$", "x");
	reject("x\\$", "$x");

	reject("x^", "x^"); // Different BRE/ERE
	reject("x^", "x");
	reject("x^", "^");
	reject("x^", "");

	reject("$x", "$x"); // Different BRE/ERE
	reject("$x", "x");
	reject("$x", "^");
	reject("$x", "");

	reject("$x^", "$x^"); // Different BRE/ERE
	reject("$x^", "x");
	reject("$x^", "^");
	reject("$x^", "$");
	reject("$x^", "x^");
	reject("$x^", "$x");
	reject("$x^", "");

	match("\\$x\\^", "$x^");
	reject("\\$x\\^", "\\$x\\^");
	reject("\\$x\\^", "x");
	reject("\\$x\\^", "^");
	reject("\\$x\\^", "$");
	reject("\\$x\\^", "x^");
	reject("\\$x\\^", "$x");
	reject("\\$x\\^", "");

	match("a\\$x\\^z", "a$x^z");
	reject("a\\$x\\^z", "a\\$x\\^z");
	reject("a\\$x\\^z", "axz");
	reject("a\\$x\\^z", "a^z");
	reject("a\\$x\\^z", "a$z");
	reject("a\\$x\\^z", "ax^z");
	reject("a\\$x\\^z", "a$xz");
	reject("a\\$x\\^z", "az");

	return 0;
}
