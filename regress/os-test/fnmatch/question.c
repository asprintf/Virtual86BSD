/* Test whether question marks work. */

#include "suite.h"

int main(void)
{
	reject("?", "");
	match("?", "a");
	reject("?", "aa");

	reject("??", "");
	reject("??", "a");
	match("??", "aa");
	reject("??", "aaa");
	match("???", "aaa");

	reject("a?", "");
	reject("a?", "a");
	reject("a?", "b");
	match("a?", "aa");
	match("a?", "ab");
	reject("a?", "ba");
	reject("a?", "bb");
	reject("a?", "aaa");

	match("a??", "aaa");
	match("a??", "aab");
	match("a??", "aba");
	match("a??", "abb");
	reject("a??", "baa");
	reject("a??", "bab");
	reject("a??", "bba");
	reject("a??", "bbb");

	match("a?a", "aaa");
	reject("a?a", "aab");
	match("a?a", "aba");
	reject("a?a", "abb");
	reject("a?a", "baa");
	reject("a?a", "bab");
	reject("a?a", "bba");
	reject("a?a", "bbb");

	match("?", "A");
	match("?", "x");
	match("?", "!");
	match("?", "\"");
	match("?", "'");
	match("?", "#");
	match("?", "$");
	match("?", "/");
	match("?", "{");
	match("?", "}");
	match("?", "(");
	match("?", ")");
	match("?", "?");
	match("?", "+");
	match("?", "|");
	match("?", "*");
	match("?", "<");
	match("?", ">");
	match("?", "\\");
	match("?", ";");
	match("?", ",");
	match("?", ".");
	match("?", ":");
	match("?", "_");
	match("?", "-");
	match("?", "/");

	return 0;
}
