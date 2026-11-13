/* Test whether brackets ranges work with backslashes. */

#include "suite.h"

int main(void)
{
	flags = 0;

	// Backslashes work inside bracket expressions and escaping a hyphen means
	// a literal hyphen instead of a range.
	match("[a\\-b]", "a");
	match("[a\\-b]", "-"); /* different from regex behavior (9.3.5 1.) */
	match("[a\\-b]", "b");
	reject("[a\\-b]", "\\");
	reject("[a\\-b]", "^");
	reject("[a\\-b]", "c");

	reject("[\\-b]", "a");
	match("[\\-b]", "-");
	match("[\\-b]", "b");
	reject("[\\-b]", "\\");
	reject("[\\-b]", "^");
	reject("[\\-b]", "c");

	match("[Z-\\]]", "Z");
	match("[Z-\\]]", "[");
	match("[Z-\\]]", "\\");
	match("[Z-\\]]", "]");
	reject("[Z-\\]]", "^");
	reject("[Z-\\]]", "a");

	match("[Z-\\\\]", "Z");
	match("[Z-\\\\]", "[");
	match("[Z-\\\\]", "\\");
	reject("[Z-\\\\]", "]");
	reject("[Z-\\\\]", "^");
	reject("[Z-\\\\]", "a");

	match("[\\\\-\\\\]", "\\");
	reject("[\\\\-\\\\]", "-");
	reject("[\\\\-\\\\]", "a");
	reject("[\\\\-\\\\]", "[");
	reject("[\\\\-\\\\]", "]");

	reject("[!\\\\-\\\\]", "\\");
	match("[!\\\\-\\\\]", "-");
	match("[!\\\\-\\\\]", "a");
	match("[!\\\\-\\\\]", "[");
	match("[!\\\\-\\\\]", "]");

	reject("[\\[-\\]]", "");
	reject("[\\[-\\]]", "-");
	match("[\\[-\\]]", "[");
	match("[\\[-\\]]", "\\");
	match("[\\[-\\]]", "]");
	reject("[\\[-\\]]", "-]");
	reject("[\\[-\\]]", "[]");
	reject("[\\[-\\]]", "\\]");
	reject("[\\[-\\]]", "]]");

	return 0;
}
