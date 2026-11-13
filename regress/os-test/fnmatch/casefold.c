/* Test whether FNM_CASEFOLD works. */

#include "suite.h"

int main(void)
{
	// Control tests to verify case sensitivity works as a baseline.
	flags = 0;

	match("a", "a");
	reject("a", "A");
	reject("a", "b");
	reject("a", "B");
	reject("A", "a");
	match("A", "A");
	reject("A", "b");
	reject("A", "B");

	match("?", "a");
	match("?", "A");

	match("[a]", "a");
	reject("[a]", "A");
	reject("[a]", "b");
	reject("[a]", "B");

	match("[aB]", "a");
	reject("[aB]", "A");
	reject("[aB]", "b");
	match("[aB]", "B");
	reject("[aB]", "c");
	reject("[aB]", "C");
	reject("[!a]", "a");
	match("[!a]", "A");
	match("[!a]", "b");
	match("[!a]", "B");

	reject("[!aB]", "a");
	match("[!aB]", "A");
	match("[!aB]", "b");
	reject("[!aB]", "B");
	match("[!aB]", "c");
	match("[!aB]", "C");

	reject("a?", "");
	reject("a?", "a");
	reject("a?", "A");
	reject("a?", "b");
	reject("a?", "B");
	match("a?", "aa");
	match("a?", "aA");
	match("a?", "ab");
	match("a?", "aB");
	reject("a?", "Aa");
	reject("a?", "AA");
	reject("a?", "Ab");
	reject("a?", "AB");
	reject("a?", "ba");
	reject("a?", "bA");
	reject("a?", "bb");
	reject("a?", "bB");
	reject("a?", "Ba");
	reject("a?", "BA");
	reject("a?", "Bb");
	reject("a?", "BB");

	reject("a*", "");
	match("a*", "a");
	reject("a*", "A");
	reject("a*", "b");
	reject("a*", "B");
	match("a*", "aa");
	match("a*", "aA");
	match("a*", "ab");
	match("a*", "aB");
	reject("a*", "Aa");
	reject("a*", "AA");
	reject("a*", "Ab");
	reject("a*", "AB");
	reject("a*", "ba");
	reject("a*", "bA");
	reject("a*", "bb");
	reject("a*", "bB");
	reject("a*", "Ba");
	reject("a*", "BA");
	reject("a*", "Bb");
	reject("a*", "BB");

	// Test that case insenstivity works.
	flags = FNM_CASEFOLD;

	match("a", "a");
	match("a", "A");
	reject("a", "b");
	reject("a", "B");
	match("A", "a");
	match("A", "A");
	reject("A", "b");
	reject("A", "B");

	match("?", "a");
	match("?", "A");

	match("[a]", "a");
	match("[a]", "A");
	reject("[a]", "b");
	reject("[a]", "B");

	match("[aB]", "a");
	match("[aB]", "A");
	match("[aB]", "b");
	match("[aB]", "B");
	reject("[aB]", "c");
	reject("[aB]", "C");

	reject("[!a]", "a");
	reject("[!a]", "A");
	match("[!a]", "b");
	match("[!a]", "B");

	reject("[!aB]", "a");
	reject("[!aB]", "A");
	reject("[!aB]", "b");
	reject("[!aB]", "B");
	match("[!aB]", "c");
	match("[!aB]", "C");

	reject("a?", "");
	reject("a?", "a");
	reject("a?", "A");
	reject("a?", "b");
	reject("a?", "B");
	match("a?", "aa");
	match("a?", "aA");
	match("a?", "ab");
	match("a?", "aB");
	match("a?", "Aa");
	match("a?", "AA");
	match("a?", "Ab");
	match("a?", "AB");
	reject("a?", "ba");
	reject("a?", "bA");
	reject("a?", "bb");
	reject("a?", "bB");
	reject("a?", "Ba");
	reject("a?", "BA");
	reject("a?", "Bb");
	reject("a?", "BB");

	reject("a*", "");
	match("a*", "a");
	match("a*", "A");
	reject("a*", "b");
	reject("a*", "B");
	match("a*", "aa");
	match("a*", "aA");
	match("a*", "ab");
	match("a*", "aB");
	match("a*", "Aa");
	match("a*", "AA");
	match("a*", "Ab");
	match("a*", "AB");
	reject("a*", "ba");
	reject("a*", "bA");
	reject("a*", "bb");
	reject("a*", "bB");
	reject("a*", "Ba");
	reject("a*", "BA");
	reject("a*", "Bb");
	reject("a*", "BB");

	return 0;
}
