/* Test whether bracket character class elements work with backslashes in
   noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	// Character classes have to be implemented in combination with backslashes
	// being regular characters in noescape mode.
	reject("\\[[:alnum:]]", "a");
	reject("\\[[:alnum:]]", "l");
	reject("\\[[:alnum:]]", "z");
	reject("\\[[:alnum:]]", "l");
	reject("\\[[:alnum:]]", "[a]");
	reject("\\[[:alnum:]]", "[l]");
	reject("\\[[:alnum:]]", "[n]");
	reject("\\[[:alnum:]]", "[u]");
	reject("\\[[:alnum:]]", "[m]");
	reject("\\[[:alnum:]]", "[:]");
	reject("\\[[:alnum:]]", "[z]");
	match("\\[[:alnum:]]", "\\a");
	reject("\\[[:alnum:]]", "\\#");

	reject("[\\[:alnum:]]", "[\\[:alnum:]]");
	match("[\\[:alnum:]]", "a");
	match("[\\[:alnum:]]", "z");
	match("[\\[:alnum:]]", "\\");
	reject("[\\[:alnum:]]", "\\]");
	reject("[\\[:alnum:]]", "[]");
	reject("[\\[:alnum:]]", ":]");
	reject("[\\[:alnum:]]", "a]");
	reject("[\\[:alnum:]]", "l]");
	reject("[\\[:alnum:]]", "n]");
	reject("[\\[:alnum:]]", "u]");
	reject("[\\[:alnum:]]", "m]");
	reject("[\\[:alnum:]]", "]]");
	reject("[\\[:alnum:]]", "z]");
	reject("[\\[:alnum:]]", "Z]");
	reject("[\\[:alnum:]]", "0]");
	reject("[\\[:alnum:]]", "#]");

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
	match("[[\\:alnum:]]", "\\]");
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
