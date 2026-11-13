/* Test whether ERE REG_NEWLINE works. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	eflags = 0;

	match("\n", "\n");
	reject("\n", "a");

	match(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "x\n", m(0, 1));
	reject("^x", "\nx");
	reject("^x", "\n\n");
	reject("^x", "\nx\n");
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	partial("^x", "xy\n", m(0, 1));
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	reject("^x", "\nxy");
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	match("x$", "x");
	reject("x$", "x\n");
	reject("^x", "y");
	partial("x$", "\nx", m(1, 2));
	reject("x$", "\n\n");
	reject("x$", "\nx\n");
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	reject("x$", "yx\n");
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	match("^x$", "x");
	reject("^x", "y");
	reject("^x$", "x\n");
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	reject("^x$", "\nx\n");
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTBOL;

	match("\n", "\n");
	reject("\n", "a");

	match(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "x\n");
	reject("^x", "\nx");
	reject("^x", "\n\n");
	reject("^x", "\nx\n");
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	reject("^x", "xy\n");
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	reject("^x", "\nxy");
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	match("x$", "x");
	reject("x$", "y");
	reject("x$", "x\n");
	partial("x$", "\nx", m(1, 2));
	reject("x$", "\n\n");
	reject("x$", "\nx\n");
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	reject("x$", "yx\n");
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "x\n");
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	reject("^x$", "\nx\n");
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTEOL;

	match("\n", "\n");
	reject("\n", "a");

	match(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "x\n", m(0, 1));
	reject("^x", "\nx");
	reject("^x", "\n\n");
	reject("^x", "\nx\n");
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	partial("^x", "xy\n", m(0, 1));
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	reject("^x", "\nxy");
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	reject("x$", "x\n");
	reject("x$", "\nx");
	reject("x$", "\n\n");
	reject("x$", "\nx\n");
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	reject("x$", "yx\n");
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "x\n");
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	reject("^x$", "\nx\n");
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTBOL | REG_NOTEOL;

	match("\n", "\n");
	reject("\n", "a");

	match(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "x\n");
	reject("^x", "\nx");
	reject("^x", "\n\n");
	reject("^x", "\nx\n");
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	reject("^x", "xy\n");
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	reject("^x", "\nxy");
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	reject("x$", "x\n");
	reject("x$", "\nx");
	reject("x$", "\n\n");
	reject("x$", "\nx\n");
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	reject("x$", "yx\n");
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "x\n");
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	reject("^x$", "\nx\n");
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	cflags = REG_EXTENDED | REG_NEWLINE;

	eflags = 0;

	match("\n", "\n");
	reject("\n", "a");

	reject(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "x\n", m(0, 1));
	partial("^x", "\nx", m(1, 2));
	reject("^x", "\n\n");
	partial("^x", "\nx\n", m(1, 2));
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	partial("^x", "xy\n", m(0, 1));
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	partial("^x", "\nxy", m(1, 2));
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	match("x$", "x");
	partial("x$", "x\n", m(0, 1));
	reject("^x", "y");
	partial("x$", "\nx", m(1, 2));
	reject("x$", "\n\n");
	partial("x$", "\nx\n", m(1, 2));
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	partial("x$", "yx\n", m(1, 2));
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	match("^x$", "x");
	reject("^x", "y");
	partial("^x$", "x\n", m(0, 1));
	partial("^x$", "\nx", m(1, 2));
	reject("^x$", "\n\n");
	partial("^x$", "\nx\n", m(1, 2));
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTBOL;

	match("\n", "\n");
	reject("\n", "a");

	reject(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "x\n");
	partial("^x", "\nx", m(1, 2));
	reject("^x", "\n\n");
	partial("^x", "\nx\n", m(1, 2));
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	reject("^x", "xy\n");
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	partial("^x", "\nxy", m(1, 2));
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	match("x$", "x");
	reject("x$", "y");
	partial("x$", "x\n", m(0, 1));
	partial("x$", "\nx", m(1, 2));
	reject("x$", "\n\n");
	partial("x$", "\nx\n", m(1, 2));
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	partial("x$", "yx\n", m(1, 2));
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "x\n");
	partial("^x$", "\nx", m(1, 2));
	reject("^x$", "\n\n");
	partial("^x$", "\nx\n", m(1, 2));
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTEOL;

	match("\n", "\n");
	reject("\n", "a");

	reject(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	match("^x", "x");
	reject("^x", "y");
	partial("^x", "x\n", m(0, 1));
	partial("^x", "\nx", m(1, 2));
	reject("^x", "\n\n");
	partial("^x", "\nx\n", m(1, 2));
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	partial("^x", "xy\n", m(0, 1));
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	partial("^x", "\nxy", m(1, 2));
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	partial("x$", "x\n", m(0, 1));
	reject("x$", "\nx");
	reject("x$", "\n\n");
	partial("x$", "\nx\n", m(1, 2));
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	partial("x$", "yx\n", m(1, 2));
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	partial("^x$", "x\n", m(0, 1));
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	partial("^x$", "\nx\n", m(1, 2));
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	eflags = REG_NOTBOL | REG_NOTEOL;

	match("\n", "\n");
	reject("\n", "a");

	reject(".", "\n");
	match(".", "a");

	reject("x", "");
	match("x", "x");
	reject("x", "y");
	partial("x", "x\n", m(0, 1));
	partial("x", "\nx", m(1, 2));
	reject("x", "\n\n");
	partial("x", "\nx\n", m(1, 2));
	reject("x", "y\n");
	reject("x", "\ny");
	reject("x", "\ny\n");
	partial("x", "xy\n", m(0, 1));
	partial("x", "yx\n", m(1, 2));
	partial("x", "yxy\n", m(1, 2));
	partial("x", "\nxy", m(1, 2));
	partial("x", "\nyxy", m(2, 3));
	partial("x", "\nyxy\n", m(2, 3));

	reject("^x", "");
	reject("^x", "x");
	reject("^x", "y");
	reject("^x", "x\n");
	partial("^x", "\nx", m(1, 2));
	reject("^x", "\n\n");
	partial("^x", "\nx\n", m(1, 2));
	reject("^x", "y\n");
	reject("^x", "\ny");
	reject("^x", "\ny\n");
	reject("^x", "xy\n");
	reject("^x", "yx\n");
	reject("^x", "yxy\n");
	partial("^x", "\nxy", m(1, 2));
	reject("^x", "\nyxy");
	reject("^x", "\nyxy\n");

	reject("x$", "");
	reject("x$", "x");
	reject("x$", "y");
	partial("x$", "x\n", m(0, 1));
	reject("x$", "\nx");
	reject("x$", "\n\n");
	partial("x$", "\nx\n", m(1, 2));
	reject("x$", "y\n");
	reject("x$", "\ny");
	reject("x$", "\ny\n");
	reject("x$", "xy\n");
	partial("x$", "yx\n", m(1, 2));
	reject("x$", "yxy\n");
	reject("x$", "\nxy");
	reject("x$", "\nyxy");
	reject("x$", "\nyxy\n");

	reject("^x$", "");
	reject("^x$", "x");
	reject("^x$", "y");
	reject("^x$", "x\n");
	reject("^x$", "\nx");
	reject("^x$", "\n\n");
	partial("^x$", "\nx\n", m(1, 2));
	reject("^x$", "y\n");
	reject("^x$", "\ny");
	reject("^x$", "\ny\n");
	reject("^x$", "xy\n");
	reject("^x$", "yx\n");
	reject("^x$", "yxy\n");
	reject("^x$", "\nxy");
	reject("^x$", "\nyxy");
	reject("^x$", "\nyxy\n");

	return 0;
}
