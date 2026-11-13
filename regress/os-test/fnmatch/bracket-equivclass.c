/* Test whether bracket equivalence class elements work. */

#include "suite.h"

int main(void)
{
	// Bracket equivalence class elements have to be implemented.
	match("[[=a=]]", "a");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "A");
	reject("[[=a=]]", "c");

	match("[[=a=][=b=]]", "a");
	match("[[=a=][=b=]]", "b");
	reject("[[=a=][=b=]]", "A");
	reject("[[=a=][=b=]]", "c");

	reject("[![=a=][=b=]]", "a");
	reject("[![=a=][=b=]]", "b");
	match("[![=a=][=b=]]", "A");
	match("[![=a=][=b=]]", "c");

	// [ is not a META_CHAR and there are no special rules for it, so it's just
	// a regular COLL_ELEM_SINGLE here, and it is allowed as an equivalence
	// class.
	reject("[[=[=]x]", "");
	reject("[[=[=]x]", ".");
	match("[[=[=]x]", "x");
	match("[[=[=]x]", "[");
	reject("[[=[=]x]", "]");
	reject("[[=[=]x]", "y");

	match("[[=\n=]]", "\n");
	reject("[[=\n=]]", " ");
	reject("[[=\n=]]", "x");

	match("[[===]]", "=");
	reject("[[===]]", "a");

	return 0;
}
