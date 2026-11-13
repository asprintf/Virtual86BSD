/* Test whether FNM_CASEFOLD works with bracket ranges. */

#include "suite.h"

int main(void)
{
	// Control tests to verify case sensitivity works as a baseline.
	flags = 0;

	match("[a-f]", "a");
	reject("[a-f]", "A");
	match("[a-f]", "d");
	reject("[a-f]", "D");
	match("[a-f]", "f");
	reject("[a-f]", "F");
	reject("[a-f]", "g");
	reject("[a-f]", "G");

	match("[A-f]", "A");
	match("[A-f]", "a");
	match("[A-f]", "D");
	match("[A-f]", "d");
	match("[A-f]", "F");
	match("[A-f]", "f");
	match("[A-f]", "G");
	reject("[A-f]", "g");
	match("[A-f]", "Z");
	reject("[A-f]", "z");
	match("[A-f]", "^");
	reject("[A-f]", "|");

	reject("[!a-f]", "a");
	match("[!a-f]", "A");
	reject("[!a-f]", "d");
	match("[!a-f]", "D");
	reject("[!a-f]", "f");
	match("[!a-f]", "F");
	match("[!a-f]", "g");
	match("[!a-f]", "G");

	reject("[!A-f]", "A");
	reject("[!A-f]", "a");
	reject("[!A-f]", "D");
	reject("[!A-f]", "d");
	reject("[!A-f]", "F");
	reject("[!A-f]", "f");
	reject("[!A-f]", "G");
	match("[!A-f]", "g");
	reject("[!A-f]", "Z");
	match("[!A-f]", "z");
	reject("[!A-f]", "^");
	match("[!A-f]", "|");

	// Test that case insenstivity works.
	flags = FNM_CASEFOLD;

	match("[a-f]", "a");
	match("[a-f]", "A");
	match("[a-f]", "d");
	match("[a-f]", "D");
	match("[a-f]", "f");
	match("[a-f]", "F");
	reject("[a-f]", "g");
	reject("[a-f]", "G");

	match("[A-f]", "A");
	match("[A-f]", "a");
	match("[A-f]", "D");
	match("[A-f]", "d");
	match("[A-f]", "F");
	match("[A-f]", "f");
	// glibc reinterpets [A-f] as [a-f] but POSIX.1-2024 sections 9.2 and 4.1
	// only talk about changing the case of the input string, not the pattern.
	// In that case, [A-f] should continue to be interpreted under ASCII as the
	// [][A-Z\]^_`a-f] pattern, and then each of those would have their both of
	// their case codepoints added.
#if 1
	match("[A-f]", "G");
	match("[A-f]", "g");
	match("[A-f]", "Z");
	match("[A-f]", "z");
	match("[A-f]", "^");
#endif
	reject("[A-f]", "|");

	reject("[!a-f]", "a");
	reject("[!a-f]", "A");
	reject("[!a-f]", "d");
	reject("[!a-f]", "D");
	reject("[!a-f]", "f");
	reject("[!a-f]", "F");
	match("[!a-f]", "g");
	match("[!a-f]", "G");

	reject("[!A-f]", "A");
	reject("[!A-f]", "a");
	reject("[!A-f]", "D");
	reject("[!A-f]", "d");
	reject("[!A-f]", "F");
	reject("[!A-f]", "f");
#if 1
	reject("[!A-f]", "G");
	reject("[!A-f]", "g");
	reject("[!A-f]", "Z");
	reject("[!A-f]", "z");
	reject("[!A-f]", "^");
#endif
	match("[!A-f]", "|");

	return 0;
}
