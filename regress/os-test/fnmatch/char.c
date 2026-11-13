/* Test basic characters. */

#include "suite.h"

int main(void)
{
	match("", "");
	reject("", "a");
	reject("", "b");
	reject("", "aa");
	reject("", "ab");
	reject("", "ba");
	reject("", "bb");

	reject("a", "");
	match("a", "a");
	reject("a", "b");
	reject("a", "A");
	reject("a", "aa");
	reject("a", "ab");
	reject("a", "ba");
	reject("a", "bb");
	reject("a", "\\a");

	reject("ab", "");
	reject("ab", "a");
	reject("ab", "b");
	reject("ab", "A");
	reject("ab", "aa");
	match("ab", "ab");
	reject("ab", "ba");
	reject("ab", "bb");
	reject("ab", "AB");
	reject("ab", "aaa");
	reject("ab", "aab");
	reject("ab", "aba");
	reject("ab", "abb");
	reject("ab", "baa");
	reject("ab", "bab");
	reject("ab", "bba");
	reject("ab", "bbb");
	reject("ab", "\\a\\b");

	reject("/", "");
	match("/", "/");
	reject("/", "#");

	reject(".", "");
	match(".", ".");
	reject(".", "a");

	match("{", "{");
	reject("{", "a");

	match("}", "}");
	reject("}", "a");

	match("(", "(");
	reject("(", "a");

	match(")", ")");
	reject(")", "a");

	match("!", "!");
	reject("!", "a");

	match("^", "^");
	reject("^", "a");

	match("]", "]");
	reject("]", "a");

	match("|", "|");
	reject("|", "a");

	match("$", "$");
	reject("$", "a");

	match("'", "'");
	reject("'", "a");

	match("\"", "\"");
	reject("\"", "a'");

	match("\n", "\n");
	reject("\n", "\t");
	reject("\n", " ");

	match("\t", "\t");
	reject("\t", "\n");
	reject("\t", " ");

	match(" ", " ");
	reject(" ", "\n");
	reject(" ", "\t");

	reject("a{2,2}", "aa");
	match("a{2,2}", "a{2,2}");

	reject("a+", "a");
	reject("a+", "aa");
	match("a+", "a+");

	reject("(a)", "a");
	match("(a)", "(a)");

	reject("a|b", "a");
	reject("a|b", "b");
	match("a|b", "a|b");

	reject("\"a\"", "a");
	match("\"a\"", "\"a\"");

	reject("'a'", "a");
	match("'a'", "'a'");

	return 0;
}
