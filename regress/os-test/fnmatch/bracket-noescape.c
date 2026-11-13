/* Test whether brackets work with backslashes in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	reject("\\[foo]", "[foo]");
	reject("\\[foo]", "\\[foo]");
	match("\\[foo]", "\\f");
	match("\\[foo]", "\\o");

	reject("[foo\\]", "[foo]");
	reject("[foo\\]", "[foo\\]");
	match("[foo\\]", "f");
	match("[foo\\]", "o");
	match("[foo\\]", "\\");

	reject("\\[foo\\]", "[foo]");
	reject("\\[foo\\]", "\\[foo]");
	reject("\\[foo\\]", "\\[foo\\]");
	match("\\[foo\\]", "\\f");
	match("\\[foo\\]", "\\o");
	match("\\[foo\\]", "\\\\");
	reject("\\[foo\\]", "\\\\f");
	reject("\\[foo\\]", "\\\\o");

	reject("\\\\[foo]", "[foo]");
	reject("\\\\[foo]", "\\[foo]");
	reject("\\\\[foo]", "\\\\[foo]");
	reject("\\\\[foo]", "\\f");
	reject("\\\\[foo]", "\\o");
	reject("\\\\[foo]", "\\\\");
	match("\\\\[foo]", "\\\\f");
	match("\\\\[foo]", "\\\\o");

	match("[x\\y]", "x");
	match("[x\\y]", "y");
	reject("[x\\y]", "z");
	match("[x\\y]", "\\");
	reject("[x\\y]", "]");

	match("[\\^x]", "x");
	reject("[\\^x]", "y");
	match("[\\^x]", "\\");
	match("[\\^x]", "^");
	reject("[\\^x]", "[\\^x]");

	match("[\\!x]", "x");
	reject("[\\!x]", "y");
	match("[\\!x]", "\\");
	match("[\\!x]", "!");
	reject("[\\!x]", "[\\!x]");

	match("[\\]", "\\");
	reject("[\\]", "]");
	reject("[\\]", "x");
	reject("[\\]", "[]");
	reject("[\\]", "[\\]");

	match("[\\\\]", "\\");
	reject("[\\\\]", "]");
	reject("[\\\\]", "x");
	reject("[\\\\]", "[\\]");
	reject("[\\\\]", "[\\\\]");

	reject("[x\\]]", "x");
	reject("[x\\]]", "y");
	reject("[x\\]]", "]");
	reject("[x\\]]", "\\");
	reject("[x\\]]", "[");
	match("[x\\]]", "x]");
	reject("[x\\]]", "y]");
	reject("[x\\]]", "]]");
	match("[x\\]]", "\\]");
	reject("[x\\]]", "[]");
	reject("[x\\]]", "[x\\]]");
	reject("[x\\]]", "[x]]");

	reject("[x\\[y]", "a");
	match("[x\\[y]", "x");
	match("[x\\[y]", "\\");
	match("[x\\[y]", "[");
	match("[x\\[y]", "y");
	reject("[x\\[y]", "z");
	reject("[x\\[y]", "]");

	reject("[x\\]y]", "a");
	reject("[x\\]y]", "x");
	reject("[x\\]y]", "\\");
	reject("[x\\]y]", "[");
	reject("[x\\]y]", "y");
	reject("[x\\]y]", "z");
	reject("[x\\]y]", "]");
	match("[x\\]y]", "xy]");
	match("[x\\]y]", "\\y]");
	reject("[x\\]y]", "zy]");

	match("[][\\]", "\\");
	match("[][\\]", "]");
	match("[][\\]", "[");
	reject("[][\\]", "x");
	reject("[][\\]", "[][]");
	match("[][\\]", "]");
	reject("[][\\]", "]]");

	reject("[][x\\]]", "");
	reject("[][x\\]]", "x");
	reject("[][x\\]]", "\\");
	reject("[][x\\]]", "]");
	reject("[][x\\]]", "[");
	reject("[][x\\]]", "y");
	match("[][x\\]]", "x]");
	match("[][x\\]]", "\\]");
	match("[][x\\]]", "]]");
	match("[][x\\]]", "[]");

	reject("[!][x\\]]", "");
	reject("[!][x\\]]", "x");
	reject("[!][x\\]]", "\\");
	reject("[!][x\\]]", "]");
	reject("[!][x\\]]", "[");
	reject("[!][x\\]]", "y");
	reject("[!][x\\]]", "x]");
	reject("[!][x\\]]", "\\]");
	reject("[!][x\\]]", "]]");
	reject("[!][x\\]]", "[]");
	match("[!][x\\]]", "y]");

	return 0;
}
