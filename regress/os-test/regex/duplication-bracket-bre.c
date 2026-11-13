/* Test whether BRE duplications work combined with brackets. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("[a-z]*", "");
	match("[a-z]*", "a");
	match("[a-z]*", "abc");
	partial("[a-z]*", "ABC", m(0, 0));
	partial("[a-z]*", "aBc", m(0, 1));
	partial("[a-z]*", "Abc", m(0, 0));

	reject("[a-z]\\{2\\}", "");
	reject("[a-z]\\{2\\}", "a");
	partial("[a-z]\\{2\\}", "abc", m(0, 2));
	reject("[a-z]\\{2\\}", "ABC");
	reject("[a-z]\\{2\\}", "aBc");
	partial("[a-z]\\{2\\}", "Abc", m(1, 3));

	return 0;
}
