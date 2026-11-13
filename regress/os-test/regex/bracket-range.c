/* Test whether brackets ranges works. */

#include "suite.h"

void run(void)
{
	match("[a-z]", "a");
	match("[a-z]", "b");
	match("[a-z]", "z");
	reject("[a-z]", "A");
	reject("[a-z]", "1");

	reject("[^a-z]", "a");
	reject("[^a-z]", "b");
	reject("[^a-z]", "z");
	match("[^a-z]", "A");
	match("[^a-z]", "1");

	match("[-az]", "-");
	match("[-az]", "a");
	reject("[-az]", "b");
	match("[-az]", "z");
	reject("[-az]", "A");

	match("[az-]", "-");
	match("[az-]", "a");
	reject("[az-]", "b");
	match("[az-]", "z");
	reject("[az-]", "A");

	match("[#-]]", "#]");
	reject("[#-]]", "#");
	reject("[#-]]", "]");
	match("[#-]]", "-]");

	match("[]#-]]", "#]");
	reject("[[#-]]", "#");
	reject("[]#-]]", "]");
	match("[]#-]]", "]]");

	match("[$]", "$");
	reject("[$]", "#");

	match("[---]", "-");
	reject("[---]", "#");

	match("[a\\-b]", "a");
	reject("[a\\-b]", "-"); /* different from shell behavior (9.3.5 1.) */
	match("[a\\-b]", "b");
	match("[a\\-b]", "\\");
	match("[a\\-b]", "^");
	reject("[a\\-b]", "c");

	match("[\\-b]", "a");
	reject("[\\-b]", "-");
	match("[\\-b]", "b");
	match("[\\-b]", "\\");
	match("[\\-b]", "^");
	reject("[\\-b]", "c");

	match("[Z-\\]", "Z");
	match("[Z-\\]", "[");
	match("[Z-\\]", "\\");
	reject("[Z-\\]", "^");
	reject("[Z-\\]", "a");

	match("[\\-\\]", "\\");
	reject("[\\-\\]", "-");
	reject("[\\-\\]", "a");
	reject("[\\-\\]", "[");
	reject("[\\-\\]", "]");

	reject("[^\\-\\]", "\\");
	match("[^\\-\\]", "-");
	match("[^\\-\\]", "a");
	match("[^\\-\\]", "[");
	match("[^\\-\\]", "]");

	reject("[\\[-\\]]", "");
	reject("[\\[-\\]]", "-");
	reject("[\\[-\\]]", "[");
	reject("[\\[-\\]]", "\\");
	reject("[\\[-\\]]", "]");
	reject("[\\[-\\]]", "-]");
	match("[\\[-\\]]", "[]");
	match("[\\[-\\]]", "\\]");
	reject("[\\[-\\]]", "]]");

	match("[]-a]", "]");
	match("[]-a]", "^");
	match("[]-a]", "a");
	reject("[]-a]", "[");
	reject("[]-a]", "-");

	reject("[^]-a]", "]");
	reject("[^]-a]", "^");
	reject("[^]-a]", "a");
	match("[^]-a]", "[");
	match("[^]-a]", "-");

	reject("[]-]]", "");
	reject("[]-]]", "[");
	reject("[]-]]", "-");
	reject("[]-]]", "]");
	reject("[]-]]", "x");
	reject("[]-]]", "[]");
	match("[]-]]", "-]");
	match("[]-]]", "]]");
	reject("[]-]]", "x]");

	reject("[^]-]]", "");
	reject("[^]-]]", "[");
	reject("[^]-]]", "-");
	reject("[^]-]]", "]");
	reject("[^]-]]", "x");
	match("[^]-]]", "[]");
	reject("[^]-]]", "-]");
	reject("[^]-]]", "]]");
	match("[^]-]]", "x]");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
