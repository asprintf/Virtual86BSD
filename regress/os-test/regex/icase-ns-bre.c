/*[nonstandard]*/
/* Test whether BRE REG_ICASE works (nonstandard). */

#include "suite.h"

int main(void)
{
	cflags = 0;

	match("a\\?", "");
	match("a\\?", "a");
	partial("a\\?", "A", m(0, 0));
	partial("a\\?", "b", m(0, 0));
	partial("a\\?", "B", m(0, 0));
	partial("a\\?", "aa", m(0, 1));
	partial("a\\?", "aA", m(0, 1));
	partial("a\\?", "ab", m(0, 1));
	partial("a\\?", "aB", m(0, 1));
	partial("a\\?", "Aa", m(0, 0));
	partial("a\\?", "AA", m(0, 0));
	partial("a\\?", "Ab", m(0, 0));
	partial("a\\?", "AB", m(0, 0));
	partial("a\\?", "ba", m(0, 0));
	partial("a\\?", "bA", m(0, 0));
	partial("a\\?", "bb", m(0, 0));
	partial("a\\?", "bB", m(0, 0));
	partial("a\\?", "Ba", m(0, 0));
	partial("a\\?", "BA", m(0, 0));
	partial("a\\?", "Bb", m(0, 0));
	partial("a\\?", "BB", m(0, 0));

	reject("a\\+", "");
	match("a\\+", "a");
	reject("a\\+", "A");
	reject("a\\+", "b");
	reject("a\\+", "B");
	match("a\\+", "aa");
	partial("a\\+", "aA", m(0, 1));
	partial("a\\+", "ab", m(0, 1));
	partial("a\\+", "aB", m(0, 1));
	partial("a\\+", "Aa", m(1, 2));
	reject("a\\+", "AA");
	reject("a\\+", "Ab");
	reject("a\\+", "AB");
	partial("a\\+", "ba", m(1, 2));
	reject("a\\+", "bA");
	reject("a\\+", "bb");
	reject("a\\+", "bB");
	partial("a\\+", "Ba", m(1, 2));
	reject("a\\+", "BA");
	reject("a\\+", "Bb");
	reject("a\\+", "BB");

	reject("a\\|B", "");
	match("a\\|B", "a");
	reject("a\\|B", "A");
	reject("a\\|B", "b");
	match("a\\|B", "B");
	reject("a\\|B", "c");
	reject("a\\|B", "C");

	cflags = 0 | REG_ICASE;

	match("a\\?", "");
	match("a\\?", "a");
	match("a\\?", "A");
	partial("a\\?", "b", m(0, 0));
	partial("a\\?", "B", m(0, 0));
	partial("a\\?", "aa", m(0, 1));
	partial("a\\?", "aA", m(0, 1));
	partial("a\\?", "ab", m(0, 1));
	partial("a\\?", "aB", m(0, 1));
	partial("a\\?", "Aa", m(0, 1));
	partial("a\\?", "AA", m(0, 1));
	partial("a\\?", "Ab", m(0, 1));
	partial("a\\?", "AB", m(0, 1));
	partial("a\\?", "ba", m(0, 0));
	partial("a\\?", "bA", m(0, 0));
	partial("a\\?", "bb", m(0, 0));
	partial("a\\?", "bB", m(0, 0));
	partial("a\\?", "Ba", m(0, 0));
	partial("a\\?", "BA", m(0, 0));
	partial("a\\?", "Bb", m(0, 0));
	partial("a\\?", "BB", m(0, 0));

	reject("a\\+", "");
	match("a\\+", "a");
	match("a\\+", "A");
	reject("a\\+", "b");
	reject("a\\+", "B");
	match("a\\+", "aa");
	match("a\\+", "aA");
	partial("a\\+", "ab", m(0, 1));
	partial("a\\+", "aB", m(0, 1));
	match("a\\+", "Aa");
	match("a\\+", "AA");
	partial("a\\+", "Ab", m(0, 1));
	partial("a\\+", "AB", m(0, 1));
	partial("a\\+", "ba", m(1, 2));
	partial("a\\+", "bA", m(1, 2));
	reject("a\\+", "bb");
	reject("a\\+", "bB");
	partial("a\\+", "Ba", m(1, 2));
	partial("a\\+", "BA", m(1, 2));
	reject("a\\+", "Bb");
	reject("a\\+", "BB");

	reject("a\\|B", "");
	match("a\\|B", "a");
	match("a\\|B", "A");
	match("a\\|B", "b");
	match("a\\|B", "B");
	reject("a\\|B", "c");
	reject("a\\|B", "C");

	return 0;
}
