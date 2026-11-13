/* Test whether ERE duplications work combined with brackets. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	match("[a-z]*", "");
	match("[a-z]*", "a");
	match("[a-z]*", "abc");
	partial("[a-z]*", "ABC", m(0, 0));
	partial("[a-z]*", "aBc", m(0, 1));
	partial("[a-z]*", "Abc", m(0, 0));

	reject("[a-z]+", "");
	match("[a-z]+", "a");
	match("[a-z]+", "abc");
	reject("[a-z]+", "ABC");
	partial("[a-z]+", "aBc", m(0, 1));
	partial("[a-z]+", "Abc", m(1, 3));

	match("[a-z]?", "");
	match("[a-z]?", "a");
	partial("[a-z]?", "abc", m(0, 1));
	partial("[a-z]?", "ABC", m(0, 0));
	partial("[a-z]?", "aBc", m(0, 1));
	partial("[a-z]?", "Abc", m(0, 0));

	return 0;
}
