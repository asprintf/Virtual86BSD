/* Test whether bracket character class elements work with backslashes. */

#include "suite.h"

int main(void)
{
	flags = 0;

	reject("\\[[:alnum:]]", "a");
	reject("\\[[:alnum:]]", "l");
	reject("\\[[:alnum:]]", "z");
	reject("\\[[:alnum:]]", "l");
	match("\\[[:alnum:]]", "[a]");
	match("\\[[:alnum:]]", "[l]");
	match("\\[[:alnum:]]", "[n]");
	match("\\[[:alnum:]]", "[u]");
	match("\\[[:alnum:]]", "[m]");
	match("\\[[:alnum:]]", "[:]");
	reject("\\[[:alnum:]]", "[z]");
	reject("\\[[:alnum:]]", "\\a");
	reject("\\[[:alnum:]]", "\\#");

	// Backslashes have to work inside bracket expressions.
	reject("[\\[:alnum:]]", "[\\[:alnum:]]");
	reject("[\\[:alnum:]]", "a");
	reject("[\\[:alnum:]]", "z");
	reject("[\\[:alnum:]]", "\\");
	reject("[\\[:alnum:]]", "\\]");
	match("[\\[:alnum:]]", "[]");
	match("[\\[:alnum:]]", ":]");
	match("[\\[:alnum:]]", "a]");
	match("[\\[:alnum:]]", "l]");
	match("[\\[:alnum:]]", "n]");
	match("[\\[:alnum:]]", "u]");
	match("[\\[:alnum:]]", "m]");
	reject("[\\[:alnum:]]", "]]");
	reject("[\\[:alnum:]]", "z]");
	reject("[\\[:alnum:]]", "Z]");
	reject("[\\[:alnum:]]", "0]");
	reject("[\\[:alnum:]]", "#]");

	reject("[\\[:alnum:]", "[\\[:alnum:]");
	reject("[\\[:alnum:]", "\\");
	match("[\\[:alnum:]", "[");
	match("[\\[:alnum:]", ":");
	match("[\\[:alnum:]", "a");
	match("[\\[:alnum:]", "l");
	match("[\\[:alnum:]", "n");
	match("[\\[:alnum:]", "u");
	match("[\\[:alnum:]", "m");
	reject("[\\[:alnum:]", "]");
	reject("[\\[:alnum:]", "z");
	reject("[\\[:alnum:]", "Z");
	reject("[\\[:alnum:]", "0");
	reject("[\\[:alnum:]", "#");

	reject("[[\\:alnum:]]", "[[\\:alnum:]]");
	reject("[[\\:alnum:]]", "[");
	reject("[[\\:alnum:]]", "\\");
	reject("[[\\:alnum:]]", ":");
	reject("[[\\:alnum:]]", "a");
	reject("[[\\:alnum:]]", "l");
	reject("[[\\:alnum:]]", "n");
	reject("[[\\:alnum:]]", "u");
	reject("[[\\:alnum:]]", "m");
	reject("[[\\:alnum:]]", "z");
	reject("[[\\:alnum:]]", "]");
	match("[[\\:alnum:]]", "[]");
	reject("[[\\:alnum:]]", "\\]");
	match("[[\\:alnum:]]", ":]");
	match("[[\\:alnum:]]", "a]");
	match("[[\\:alnum:]]", "l]");
	match("[[\\:alnum:]]", "n]");
	match("[[\\:alnum:]]", "u]");
	match("[[\\:alnum:]]", "m]");
	reject("[[\\:alnum:]]", "z]");
	reject("[[\\:alnum:]]", "]]");

	return 0;
}
