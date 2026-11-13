/* Test whether ERE REG_NOTBOL and REG_NOTEOL work. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	eflags = 0;

	reject("x", "");
	match("x", "x");
	reject("x", "y");

	match("^", "");
	partial("^", "x", m(0, 0));
	partial("^", "y", m(0, 0));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "xx", m(0, 1));
	partial("^x", "xy", m(0, 1));
	reject("^x", "yx");
	reject("^x", "yy");

	match("$", "");
	partial("$", "x", m(1, 1));
	partial("$", "y", m(1, 1));

	reject("x$", "");
	match("x$", "x");
	reject("x$", "y");
	partial("x$", "xx", m(1, 2));
	reject("x$", "xy");
	partial("x$", "yx", m(1, 2));
	reject("x$", "yy");

	match("^$", "");
	reject("^$", "x");
	reject("^$", "y");

	reject("^x$", "");
	match("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "xx");
	reject("^x$", "xy");
	reject("^x$", "yx");
	reject("^x$", "yy");

	eflags = REG_NOTBOL;

	reject("x", "");
	match("x", "x");
	reject("x", "y");

	reject("^", "");
	reject("^", "x");
	reject("^", "y");

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "xx");
	reject("^x", "xy");
	reject("^x", "yx");
	reject("^x", "yy");

	match("$", "");
	partial("$", "x", m(1, 1));
	partial("$", "y", m(1, 1));

	reject("x$", "");
	match("x$", "x");
	reject("x$", "y");
	partial("x$", "xx", m(1, 2));
	reject("x$", "xy");
	partial("x$", "yx", m(1, 2));
	reject("x$", "yy");

	reject("^$", "");
	reject("^$", "x");
	reject("^$", "y");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "xx");
	reject("^x$", "xy");
	reject("^x$", "yx");
	reject("^x$", "yy");

	eflags = REG_NOTEOL;

	reject("x", "");
	match("x", "x");
	reject("x", "y");

	match("^", "");
	partial("^", "x", m(0, 0));
	partial("^", "y", m(0, 0));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "xx", m(0, 1));
	partial("^x", "xy", m(0, 1));
	reject("^x", "yx");
	reject("^x", "yy");

	reject("$", "");
	reject("$", "x");
	reject("$", "y");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	reject("x$", "xx");
	reject("x$", "xy");
	reject("x$", "yx");
	reject("x$", "yy");

	reject("^$", "");
	reject("^$", "x");
	reject("^$", "y");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "xx");
	reject("^x$", "xy");
	reject("^x$", "yx");
	reject("^x$", "yy");

	eflags = REG_NOTBOL | REG_NOTEOL;

	reject("x", "");
	match("x", "x");
	reject("x", "y");

	reject("^", "");
	reject("^", "x");
	reject("^", "y");

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "xx");
	reject("^x", "xy");
	reject("^x", "yx");
	reject("^x", "yy");

	reject("$", "");
	reject("$", "x");
	reject("$", "y");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	reject("x$", "xx");
	reject("x$", "xy");
	reject("x$", "yx");
	reject("x$", "yy");

	reject("^$", "");
	reject("^$", "x");
	reject("^$", "y");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "xx");
	reject("^x$", "xy");
	reject("^x$", "yx");
	reject("^x$", "yy");

	return 0;
}
