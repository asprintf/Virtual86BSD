/* Test whether bracket equivalence class elements work with backslashes in
   noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	// Control test that bracket equivalence class elements work.
	match("[[=a=]]", "a");
	reject("[[=a=]]", "#");

	match("[\\[=a=]]", "\\");
	match("[\\[=a=]]", "a");
	reject("[\\[=a=]]", "b");
	reject("[\\[=a=]]", "=");
	reject("[\\[=a=]]", "[");
	reject("[\\[=a=]]", "]");
	reject("[\\[=a=]]", "a]");
	reject("[\\[=a=]]", "b]");
	reject("[\\[=a=]]", "=]");
	reject("[\\[=a=]]", "[]");
	reject("[\\[=a=]]", "]]");

	match("[[=\\=]]", "\\");
	reject("[[=\\=]]", "#");

	return 0;
}
