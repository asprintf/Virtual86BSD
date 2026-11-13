/* Test whether bracket character class elements works. */

#include "suite.h"

void run(void)
{
	match("[[:alnum:]]", "a");
	match("[[:alnum:]]", "Z");
	match("[[:alnum:]]", "0");
	reject("[[:alnum:]]", "#");
	reject("[^[:alnum:]]", "a");
	reject("[^[:alnum:]]", "Z");
	reject("[^[:alnum:]]", "0");
	match("[^[:alnum:]]", "#");

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

	match("[\\[:alnum:]]", "\\");
	match("[\\[:alnum:]]", "a");
	match("[\\[:alnum:]]", "Z");
	match("[\\[:alnum:]]", "0");
	reject("[\\[:alnum:]]", "#");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
