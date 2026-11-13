/* Test whether bracket equivalence class elements work with backslashes. */

#include "suite.h"

int main(void)
{
	flags = 0;

	// Control test that bracket equivalence class elements work.
	match("[[=a=]]", "a");
	reject("[[=a=]]", "#");

	reject("[\\[=a=]]", "\\");
	reject("[\\[=a=]]", "a");
	reject("[\\[=a=]]", "b");
	reject("[\\[=a=]]", "=");
	reject("[\\[=a=]]", "[");
	reject("[\\[=a=]]", "]");
	match("[\\[=a=]]", "a]");
	reject("[\\[=a=]]", "b]");
	match("[\\[=a=]]", "=]");
	match("[\\[=a=]]", "[]");
	reject("[\\[=a=]]", "]]");

	// Okay we are in wild territory like bracket-charclass-escape-mid.c:
	//
	// I don't think anybody ever considered this case and it works on no
	// existing operating system. However POSIX.1-2024 2.14.1 is quite clear:
	//
	// "In a pattern, or part of one, where a shell-quoting <backslash> can be
	//  used, a <backslash> character shall escape the following character as
	//  described in 2.2.1 Escape Character (Backslash), regardless of whether
	//  or not the <backslash> is inside a bracket expression. "
	//
	// POSIX expressly clarifies patterns like [\x] [\]] do escape in its
	// examples. There are no special exemptions defined for the [[:foo:]],
	// [[=x=]] and [[.x.]] syntaxes with regards to backslashes. fnmatch is
	// supposed to implement shell escaping throughout the whole string, where
	// \x means a literal x for all characters x.
	//
	// The backslash escaping needs to be supported, otherwise there is no way
	// to use the backslash equivalence class in the shell pattern language.
	//
	// My conclusion is that [[=\x=]] must be equivalent to [[=x=]].
	//
	// We may need an official POSIX resolution here.
	match("[[=\\a=]]", "a");
	reject("[[=\\a=]]", "#");

	match("[[=\\==]]", "=");
	reject("[[=\\==]]", "#");

	match("[[=\\a=]]", "a");
	reject("[[=\\a=]]", "#");

	match("[[=\\\\=]]", "\\");
	reject("[[=\\\\=]]", "=");
	reject("[[=\\\\=]]", "[");
	reject("[[=\\\\=]]", "]");
	reject("[[=\\\\=]]", "a");

	match("[[=\\]=]]", "]");
	reject("[[=\\]=]]", "#");
	reject("[[=\\]=]]", "=");

	match("[[=\\[=]]", "[");
	reject("[[=\\[=]]", "#");
	reject("[[=\\[=]]", "=");

	return 0;
}
