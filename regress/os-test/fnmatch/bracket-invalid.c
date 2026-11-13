/* Test whether invalid bracket sequences fall back to [ matching [. */

#include "suite.h"

int main(void)
{
	flags = 0;

	// Invalid bracket expressions fall back to [ matching [. It does not return
	// an unexpected error code.
	reject("[foo", "f");
	reject("[foo", "o");
	reject("[foo", "fo");
	reject("[foo", "foo");
	reject("[foo", "oo");
	reject("[foo", "foo]");
	match("[foo", "[foo");
	reject("[foo", "[foo]");

	// Backslashes escape inside brackets, even for the right square bracket
	// Bracket expressions must end in an unescaped right square bracket.
	// Invalid bracket expressions are ignored and the leading left square
	// bracket matches its own '[' character inside the input string only, and
	// then the pattern continues from foo\] onwards. \] is an escape sequence
	// that matches ]. Therefore the [foo\] matches only [foo].
	match("[foo\\]", "[foo]");
	reject("[foo\\]", "[foo\\]");
	// This is an invalid bracket expression. It should not match prematurely if
	// the brackets are unbalanced.
	reject("[foo\\]", "f");
	reject("[foo\\]", "o");
	reject("[foo\\]", "\\");

	reject("[]", "\\");
	reject("[]", "]");
	reject("[]", "x");
	match("[]", "[]");
	reject("[]", "[\\]");

	reject("[\\]", "\\");
	reject("[\\]", "]");
	reject("[\\]", "x");
	match("[\\]", "[]");
	reject("[\\]", "[\\]");

	reject("[][\\]", "\\");
	reject("[][\\]", "]");
	reject("[][\\]", "[");
	reject("[][\\]", "x");
	match("[][\\]", "[][]");
	reject("[][\\]", "]");
	reject("[][\\]", "]]");

	reject("[Z-\\]", "Z");
	reject("[Z-\\]", "[");
	reject("[Z-\\]", "\\");
	reject("[Z-\\]", "^");
	reject("[Z-\\]", "a");
	match("[Z-\\]", "[Z-]");

	flags = FNM_NOESCAPE;

	reject("[foo", "f");
	reject("[foo", "o");
	reject("[foo", "fo");
	reject("[foo", "foo");
	reject("[foo", "oo");
	reject("[foo", "foo]");
	match("[foo", "[foo");
	reject("[foo", "[foo]");

	reject("[]", "\\");
	reject("[]", "]");
	reject("[]", "x");
	match("[]", "[]");
	reject("[]", "[\\]");

	return 0;
}
