/* Test whether bracket character class elements work. */

#include "suite.h"

int main(void)
{
	// Bracket character classes have to be implemented.
	match("[[:alnum:]]", "a");
	match("[[:alnum:]]", "Z");
	match("[[:alnum:]]", "0");
	reject("[[:alnum:]]", "#");
	reject("[![:alnum:]]", "a");
	reject("[![:alnum:]]", "Z");
	reject("[![:alnum:]]", "0");
	match("[![:alnum:]]", "#");

	match("[[:cntrl:]]", "\n");
	reject("[[:cntrl:]]", "n");

	match("[[:lower:]]", "a");
	reject("[[:lower:]]", "A");

	match("[[:upper:]]", "A");
	reject("[[:upper:]]", "a");

	match("[[:space:]]", " ");
	reject("[[:space:]]", "_");

	match("[[:alpha:]]", "A");
	reject("[[:alpha:]]", "1");

	match("[[:digit:]]", "1");
	reject("[[:digit:]]", "A");

	match("[[:print:]]", " ");
	reject("[[:print:]]", "\n");

	match("[[:upper:]]", "A");
	reject("[[:upper:]]", "a");

	match("[[:blank:]]", "\t");
	reject("[[:blank:]]", "\n");

	match("[[:graph:]]", "!");
	reject("[[:graph:]]", " ");

	match("[[:punct:]]", "!");
	reject("[[:punct:]]", "0");

	match("[[:xdigit:]]", "F");
	reject("[[:xdigit:]]", "G");

	return 0;
}
