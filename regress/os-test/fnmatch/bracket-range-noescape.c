/* Test whether brackets ranges work in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	match("[a\\-b]", "a");
	reject("[a\\-b]", "-");
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

	reject("[Z-\\]]", "Z");
	reject("[Z-\\]]", "[");
	reject("[Z-\\]]", "\\");
	reject("[Z-\\]]", "]");
	reject("[Z-\\]]", "^");
	reject("[Z-\\]]", "a");
	match("[Z-\\]]", "Z]");
	match("[Z-\\]]", "[]");
	match("[Z-\\]]", "\\]");
	reject("[Z-\\]]", "]]");
	reject("[Z-\\]]", "^]");
	reject("[Z-\\]]", "a]");

	match("[Z-\\\\]", "Z");
	match("[Z-\\\\]", "[");
	match("[Z-\\\\]", "\\");
	reject("[Z-\\\\]", "]");
	reject("[Z-\\\\]", "^");
	reject("[Z-\\\\]", "a");

	match("[\\-\\]", "\\");
	reject("[\\-\\]", "-");
	reject("[\\-\\]", "a");
	reject("[\\-\\]", "[");
	reject("[\\-\\]", "]");

	reject("[!\\-\\]", "\\");
	match("[!\\-\\]", "-");
	match("[!\\-\\]", "a");
	match("[!\\-\\]", "[");
	match("[!\\-\\]", "]");

	reject("[\\[-\\]]", "");
	reject("[\\[-\\]]", "-");
	reject("[\\[-\\]]", "[");
	reject("[\\[-\\]]", "\\");
	reject("[\\[-\\]]", "]");
	reject("[\\[-\\]]", "-]");
	match("[\\[-\\]]", "[]");
	match("[\\[-\\]]", "\\]");
	reject("[\\[-\\]]", "]]");

	return 0;
}
