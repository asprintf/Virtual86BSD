/* Test whether ERE REG_ICASE works. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	match("a", "a");
	reject("a", "A");
	reject("a", "b");
	reject("a", "B");
	reject("A", "a");
	match("A", "A");
	reject("A", "b");
	reject("A", "B");

	match(".", "a");
	match(".", "A");

	match("^a", "a");
	reject("^a", "A");
	reject("^a", "b");
	reject("^a", "B");
	reject("^A", "a");
	match("^A", "A");
	reject("^A", "b");
	reject("^A", "B");

	match("a$", "a");
	reject("a$", "A");
	reject("a$", "b");
	reject("a$", "B");
	reject("A$", "a");
	match("A$", "A");
	reject("A$", "b");
	reject("A$", "B");

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

	match("[[.a.]]", "a");
	reject("[[.a.]]", "A");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "B");

	match("[[.A.]-[.f.]]", "A");
	match("[[.A.]-[.f.]]", "a");
	match("[[.A.]-[.f.]]", "D");
	match("[[.A.]-[.f.]]", "d");
	match("[[.A.]-[.f.]]", "F");
	match("[[.A.]-[.f.]]", "f");
	match("[[.A.]-[.f.]]", "G");
	reject("[[.A.]-[.f.]]", "g");
	match("[[.A.]-[.f.]]", "Z");
	reject("[[.A.]-[.f.]]", "z");
	match("[[.A.]-[.f.]]", "^");
	reject("[[.A.]-[.f.]]", "|");

	match("[[=a=]]", "a");
	reject("[[=a=]]", "A");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "B");

	reject("[^a]", "a");
	match("[^a]", "A");
	match("[^a]", "b");
	match("[^a]", "B");

	reject("[^aB]", "a");
	match("[^aB]", "A");
	match("[^aB]", "b");
	reject("[^aB]", "B");
	match("[^aB]", "c");
	match("[^aB]", "C");

	reject("[^a-f]", "a");
	match("[^a-f]", "A");
	reject("[^a-f]", "d");
	match("[^a-f]", "D");
	reject("[^a-f]", "f");
	match("[^a-f]", "F");
	match("[^a-f]", "g");
	match("[^a-f]", "G");

	reject("[^A-f]", "A");
	reject("[^A-f]", "a");
	reject("[^A-f]", "D");
	reject("[^A-f]", "d");
	reject("[^A-f]", "F");
	reject("[^A-f]", "f");
	reject("[^A-f]", "G");
	match("[^A-f]", "g");
	reject("[^A-f]", "Z");
	match("[^A-f]", "z");
	reject("[^A-f]", "^");
	match("[^A-f]", "|");

	reject("[^[.a.]]", "a");
	match("[^[.a.]]", "A");
	match("[^[.a.]]", "b");
	match("[^[.a.]]", "B");

	reject("[^[.A.]-[.f.]]", "A");
	reject("[^[.A.]-[.f.]]", "a");
	reject("[^[.A.]-[.f.]]", "D");
	reject("[^[.A.]-[.f.]]", "d");
	reject("[^[.A.]-[.f.]]", "F");
	reject("[^[.A.]-[.f.]]", "f");
	reject("[^[.A.]-[.f.]]", "G");
	match("[^[.A.]-[.f.]]", "g");
	reject("[^[.A.]-[.f.]]", "Z");
	match("[^[.A.]-[.f.]]", "z");
	reject("[^[.A.]-[.f.]]", "^");
	match("[^[.A.]-[.f.]]", "|");

	reject("[^[=a=]]", "a");
	match("[^[=a=]]", "A");
	match("[^[=a=]]", "b");
	match("[^[=a=]]", "B");

	match("a?", "");
	match("a?", "a");
	partial("a?", "A", m(0, 0));
	partial("a?", "b", m(0, 0));
	partial("a?", "B", m(0, 0));
	partial("a?", "aa", m(0, 1));
	partial("a?", "aA", m(0, 1));
	partial("a?", "ab", m(0, 1));
	partial("a?", "aB", m(0, 1));
	partial("a?", "Aa", m(0, 0));
	partial("a?", "AA", m(0, 0));
	partial("a?", "Ab", m(0, 0));
	partial("a?", "AB", m(0, 0));
	partial("a?", "ba", m(0, 0));
	partial("a?", "bA", m(0, 0));
	partial("a?", "bb", m(0, 0));
	partial("a?", "bB", m(0, 0));
	partial("a?", "Ba", m(0, 0));
	partial("a?", "BA", m(0, 0));
	partial("a?", "Bb", m(0, 0));
	partial("a?", "BB", m(0, 0));

	match("a*", "");
	match("a*", "a");
	partial("a*", "A", m(0, 0));
	partial("a*", "b", m(0, 0));
	partial("a*", "B", m(0, 0));
	match("a*", "aa");
	partial("a*", "aA", m(0, 1));
	partial("a*", "ab", m(0, 1));
	partial("a*", "aB", m(0, 1));
	partial("a*", "Aa", m(0, 0));
	partial("a*", "AA", m(0, 0));
	partial("a*", "Ab", m(0, 0));
	partial("a*", "AB", m(0, 0));
	partial("a*", "ba", m(0, 0));
	partial("a*", "bA", m(0, 0));
	partial("a*", "bb", m(0, 0));
	partial("a*", "bB", m(0, 0));
	partial("a*", "Ba", m(0, 0));
	partial("a*", "BA", m(0, 0));
	partial("a*", "Bb", m(0, 0));
	partial("a*", "BB", m(0, 0));

	reject("a+", "");
	match("a+", "a");
	reject("a+", "A");
	reject("a+", "b");
	reject("a+", "B");
	match("a+", "aa");
	partial("a+", "aA", m(0, 1));
	partial("a+", "ab", m(0, 1));
	partial("a+", "aB", m(0, 1));
	partial("a+", "Aa", m(1, 2));
	reject("a+", "AA");
	reject("a+", "Ab");
	reject("a+", "AB");
	partial("a+", "ba", m(1, 2));
	reject("a+", "bA");
	reject("a+", "bb");
	reject("a+", "bB");
	partial("a+", "Ba", m(1, 2));
	reject("a+", "BA");
	reject("a+", "Bb");
	reject("a+", "BB");

	reject("a{2}", "");
	reject("a{2}", "a");
	reject("a{2}", "A");
	reject("a{2}", "b");
	reject("a{2}", "B");
	match("a{2}", "aa");
	reject("a{2}", "aA");
	reject("a{2}", "ab");
	reject("a{2}", "aB");
	reject("a{2}", "Aa");
	reject("a{2}", "AA");
	reject("a{2}", "Ab");
	reject("a{2}", "AB");
	reject("a{2}", "ba");
	reject("a{2}", "bA");
	reject("a{2}", "bb");
	reject("a{2}", "bB");
	reject("a{2}", "Ba");
	reject("a{2}", "BA");
	reject("a{2}", "Bb");
	reject("a{2}", "BB");

	matches("(a)", "a", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("(a)", "A");
	reject("(a)", "b");
	reject("(a)", "B");
	reject("(A)", "a");
	matches("(A)", "A", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("(A)", "b");
	reject("(A)", "B");

	reject("a|B", "");
	match("a|B", "a");
	reject("a|B", "A");
	reject("a|B", "b");
	match("a|B", "B");
	reject("a|B", "c");
	reject("a|B", "C");

	cflags = REG_EXTENDED | REG_ICASE;

	match("a", "a");
	match("a", "A");
	reject("a", "b");
	reject("a", "B");
	match("A", "a");
	match("A", "A");
	reject("A", "b");
	reject("A", "B");

	match(".", "a");
	match(".", "A");

	match("^a", "a");
	match("^a", "A");
	reject("^a", "b");
	reject("^a", "B");
	match("^A", "a");
	match("^A", "A");
	reject("^A", "b");
	reject("^A", "B");

	match("a$", "a");
	match("a$", "A");
	reject("a$", "b");
	reject("a$", "B");
	match("A$", "a");
	match("A$", "A");
	reject("A$", "b");
	reject("A$", "B");

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

	match("[[.a.]]", "a");
	match("[[.a.]]", "A");
	reject("[[.a.]]", "b");
	reject("[[.a.]]", "B");

	match("[[.A.]-[.f.]]", "A");
	match("[[.A.]-[.f.]]", "a");
	match("[[.A.]-[.f.]]", "D");
	match("[[.A.]-[.f.]]", "d");
	match("[[.A.]-[.f.]]", "F");
	match("[[.A.]-[.f.]]", "f");
#if 1
	match("[[.A.]-[.f.]]", "G");
	match("[[.A.]-[.f.]]", "g");
	match("[[.A.]-[.f.]]", "Z");
	match("[[.A.]-[.f.]]", "z");
	match("[[.A.]-[.f.]]", "^");
#endif
	reject("[[.A.]-[.f.]]", "|");

	match("[[=a=]]", "a");
	match("[[=a=]]", "A");
	reject("[[=a=]]", "b");
	reject("[[=a=]]", "B");

	reject("[^a]", "a");
	reject("[^a]", "A");
	match("[^a]", "b");
	match("[^a]", "B");

	reject("[^aB]", "a");
	reject("[^aB]", "A");
	reject("[^aB]", "b");
	reject("[^aB]", "B");
	match("[^aB]", "c");
	match("[^aB]", "C");

	reject("[^a-f]", "a");
	reject("[^a-f]", "A");
	reject("[^a-f]", "d");
	reject("[^a-f]", "D");
	reject("[^a-f]", "f");
	reject("[^a-f]", "F");
	match("[^a-f]", "g");
	match("[^a-f]", "G");

	reject("[^A-f]", "A");
	reject("[^A-f]", "a");
	reject("[^A-f]", "D");
	reject("[^A-f]", "d");
	reject("[^A-f]", "F");
	reject("[^A-f]", "f");
#if 1
	reject("[^A-f]", "G");
	reject("[^A-f]", "g");
	reject("[^A-f]", "Z");
	reject("[^A-f]", "z");
	reject("[^A-f]", "^");
#endif
	match("[^A-f]", "|");

	reject("[^[.a.]]", "a");
	reject("[^[.a.]]", "A");
	match("[^[.a.]]", "b");
	match("[^[.a.]]", "B");

	reject("[^[.A.]-[.f.]]", "A");
	reject("[^[.A.]-[.f.]]", "a");
	reject("[^[.A.]-[.f.]]", "D");
	reject("[^[.A.]-[.f.]]", "d");
	reject("[^[.A.]-[.f.]]", "F");
	reject("[^[.A.]-[.f.]]", "f");
#if 1
	reject("[^[.A.]-[.f.]]", "G");
	reject("[^[.A.]-[.f.]]", "g");
	reject("[^[.A.]-[.f.]]", "Z");
	reject("[^[.A.]-[.f.]]", "z");
	reject("[^[.A.]-[.f.]]", "^");
#endif
	match("[^[.A.]-[.f.]]", "|");

	reject("[^[=a=]]", "a");
	reject("[^[=a=]]", "A");
	match("[^[=a=]]", "b");
	match("[^[=a=]]", "B");

	match("a?", "");
	match("a?", "a");
	match("a?", "A");
	partial("a?", "b", m(0, 0));
	partial("a?", "B", m(0, 0));
	partial("a?", "aa", m(0, 1));
	partial("a?", "aA", m(0, 1));
	partial("a?", "ab", m(0, 1));
	partial("a?", "aB", m(0, 1));
	partial("a?", "Aa", m(0, 1));
	partial("a?", "AA", m(0, 1));
	partial("a?", "Ab", m(0, 1));
	partial("a?", "AB", m(0, 1));
	partial("a?", "ba", m(0, 0));
	partial("a?", "bA", m(0, 0));
	partial("a?", "bb", m(0, 0));
	partial("a?", "bB", m(0, 0));
	partial("a?", "Ba", m(0, 0));
	partial("a?", "BA", m(0, 0));
	partial("a?", "Bb", m(0, 0));
	partial("a?", "BB", m(0, 0));

	match("a*", "");
	match("a*", "a");
	match("a*", "A");
	partial("a*", "b", m(0, 0));
	partial("a*", "B", m(0, 0));
	match("a*", "aa");
	match("a*", "aA");
	partial("a*", "ab", m(0, 1));
	partial("a*", "aB", m(0, 1));
	match("a*", "Aa");
	match("a*", "AA");
	partial("a*", "Ab", m(0, 1));
	partial("a*", "AB", m(0, 1));
	partial("a*", "ba", m(0, 0));
	partial("a*", "bA", m(0, 0));
	partial("a*", "bb", m(0, 0));
	partial("a*", "bB", m(0, 0));
	partial("a*", "Ba", m(0, 0));
	partial("a*", "BA", m(0, 0));
	partial("a*", "Bb", m(0, 0));
	partial("a*", "BB", m(0, 0));

	reject("a+", "");
	match("a+", "a");
	match("a+", "A");
	reject("a+", "b");
	reject("a+", "B");
	match("a+", "aa");
	match("a+", "aA");
	partial("a+", "ab", m(0, 1));
	partial("a+", "aB", m(0, 1));
	match("a+", "Aa");
	match("a+", "AA");
	partial("a+", "Ab", m(0, 1));
	partial("a+", "AB", m(0, 1));
	partial("a+", "ba", m(1, 2));
	partial("a+", "bA", m(1, 2));
	reject("a+", "bb");
	reject("a+", "bB");
	partial("a+", "Ba", m(1, 2));
	partial("a+", "BA", m(1, 2));
	reject("a+", "Bb");
	reject("a+", "BB");

	reject("a{2}", "");
	reject("a{2}", "a");
	reject("a{2}", "A");
	reject("a{2}", "b");
	reject("a{2}", "B");
	match("a{2}", "aa");
	match("a{2}", "aA");
	reject("a{2}", "ab");
	reject("a{2}", "aB");
	match("a{2}", "Aa");
	match("a{2}", "AA");
	reject("a{2}", "Ab");
	reject("a{2}", "AB");
	reject("a{2}", "ba");
	reject("a{2}", "bA");
	reject("a{2}", "bb");
	reject("a{2}", "bB");
	reject("a{2}", "Ba");
	reject("a{2}", "BA");
	reject("a{2}", "Bb");
	reject("a{2}", "BB");

	matches("(a)", "a", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("(a)", "A", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("(a)", "b");
	reject("(a)", "B");
	matches("(A)", "a", 2, (M[]) { m(0, 1), m(0, 1) });
	matches("(A)", "A", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("(A)", "b");
	reject("(A)", "B");

	reject("a|B", "");
	match("a|B", "a");
	match("a|B", "A");
	match("a|B", "b");
	match("a|B", "B");
	reject("a|B", "c");
	reject("a|B", "C");

	return 0;
}
