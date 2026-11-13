/* Test whether brackets work with backslashes. */

#include "suite.h"

int main(void)
{
	flags = 0;

	match("\\[foo]", "[foo]");
	reject("\\[foo]", "\\[foo]");
	reject("\\[foo]", "\\f");
	reject("\\[foo]", "\\o");

	match("\\[foo\\]", "[foo]");
	reject("\\[foo\\]", "\\[foo]");
	reject("\\[foo\\]", "\\[foo\\]");
	reject("\\[foo\\]", "\\f");
	reject("\\[foo\\]", "\\o");
	reject("\\[foo\\]", "\\\\");
	reject("\\[foo\\]", "\\\\f");
	reject("\\[foo\\]", "\\\\o");

	reject("\\\\[foo]", "[foo]");
	reject("\\\\[foo]", "\\[foo]");
	reject("\\\\[foo]", "\\\\[foo]");
	match("\\\\[foo]", "\\f");
	match("\\\\[foo]", "\\o");
	reject("\\\\[foo]", "\\\\");
	reject("\\\\[foo]", "\\\\f");
	reject("\\\\[foo]", "\\\\o");

	match("[x\\y]", "x");
	match("[x\\y]", "y");
	reject("[x\\y]", "z");
	// Backslash is an escape character inside brackets.
	reject("[x\\y]", "\\");
	reject("[x\\y]", "]");

	match("[\\^x]", "x");
	reject("[\\^x]", "y");
	reject("[\\^x]", "\\");
	match("[\\^x]", "^");
	reject("[\\^x]", "[\\^x]");

	match("[\\!x]", "x");
	reject("[\\!x]", "y");
	reject("[\\!x]", "\\");
	match("[\\!x]", "!");
	reject("[\\!x]", "[\\!x]");

	match("[\\\\]", "\\");
	reject("[\\\\]", "]");
	reject("[\\\\]", "x");
	reject("[\\\\]", "[\\]");
	reject("[\\\\]", "[\\\\]");

	// Backslash is an escape character inside brackets even if a right square
	// bracket follows it.
	match("[x\\]]", "x");
	reject("[x\\]]", "y");
	match("[x\\]]", "]");
	reject("[x\\]]", "\\");
	reject("[x\\]]", "[");
	reject("[x\\]]", "x]");
	reject("[x\\]]", "y]");
	reject("[x\\]]", "]]");
	reject("[x\\]]", "\\]");
	reject("[x\\]]", "[]");
	reject("[x\\]]", "[x\\]]");
	reject("[x\\]]", "[x]]");

	reject("[x\\[y]", "a");
	match("[x\\[y]", "x");
	reject("[x\\[y]", "\\");
	match("[x\\[y]", "[");
	match("[x\\[y]", "y");
	reject("[x\\[y]", "z");
	reject("[x\\[y]", "]");

	reject("[x\\]y]", "a");
	match("[x\\]y]", "x");
	reject("[x\\]y]", "\\");
	reject("[x\\]y]", "[");
	match("[x\\]y]", "y");
	reject("[x\\]y]", "z");
	match("[x\\]y]", "]");
	reject("[x\\]y]", "xy]");
	reject("[x\\]y]", "\\y]");
	reject("[x\\]y]", "zy]");

	reject("[][x\\]]", "");
	match("[][x\\]]", "x");
	reject("[][x\\]]", "\\");
	match("[][x\\]]", "]");
	match("[][x\\]]", "[");
	reject("[][x\\]]", "y");
	reject("[][x\\]]", "x]");
	reject("[][x\\]]", "\\]");
	reject("[][x\\]]", "]]");
	reject("[][x\\]]", "[]");

	reject("[!][x\\]]", "");
	reject("[!][x\\]]", "x");
	match("[!][x\\]]", "\\");
	reject("[!][x\\]]", "]");
	reject("[!][x\\]]", "[");
	match("[!][x\\]]", "y");
	reject("[!][x\\]]", "x]");
	reject("[!][x\\]]", "\\]");
	reject("[!][x\\]]", "]]");
	reject("[!][x\\]]", "[]");
	reject("[!][x\\]]", "y]");

	return 0;
}
