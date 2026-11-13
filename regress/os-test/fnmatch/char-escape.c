/* Test whether backslashes work with basic characters. */

#include "suite.h"

int main(void)
{
	flags = 0;

	reject("\\\\", "");
	match("\\\\", "\\");
	reject("\\\\", "a");
	reject("\\\\", "\\\\");

	reject("\\a", "");
	match("\\a", "a");
	reject("\\a", "b");
	reject("\\a", "aa");
	reject("\\a", "\\a\\b");
	reject("\\a", "ba");
	reject("\\a", "bb");
	reject("\\a", "\\a");

	reject("\\a\\b", "");
	reject("\\a\\b", "a");
	reject("\\a\\b", "b");
	reject("\\a\\b", "aa");
	match("\\a\\b", "ab");
	reject("\\a\\b", "ba");
	reject("\\a\\b", "bb");
	reject("\\a\\b", "aaa");
	reject("\\a\\b", "aab");
	reject("\\a\\b", "aba");
	reject("\\a\\b", "abb");
	reject("\\a\\b", "baa");
	reject("\\a\\b", "bab");
	reject("\\a\\b", "bba");
	reject("\\a\\b", "bbb");
	reject("\\a\\b", "\\a\\b");

	reject("\\/", "");
	match("\\/", "/");
	reject("\\/", "\\/");
	reject("\\/", "#");

	reject("\\.", "");
	match("\\.", ".");
	reject("\\.", "\\.");
	reject("\\.", "a");

	match("\\{", "{");
	reject("\\{", "\\{");
	reject("\\{", "a");

	match("\\}", "}");
	reject("\\}", "\\}");
	reject("\\}", "a");

	match("\\(", "(");
	reject("\\(", "\\(");
	reject("\\(", "a");

	match("\\)", ")");
	reject("\\)", "\\)");
	reject("\\)", "a");

	match("\\!", "!");
	reject("\\!", "\\!");
	reject("\\!", "a");

	match("\\^", "^");
	reject("\\^", "\\^");
	reject("\\^", "a");

	match("\\]", "]");
	reject("\\]", "\\]");
	reject("\\]", "a");

	match("\\|", "|");
	reject("\\|", "\\|");
	reject("\\|", "a");

	match("\\*", "*");
	reject("\\*", "\\*");
	reject("\\*", "a");

	match("\\?", "?");
	reject("\\?", "\\?");
	reject("\\?", "a");

	match("\\[", "[");
	reject("\\[", "\\[");
	reject("\\[", "a");

	match("\\\n", "\n");
	reject("\\\n", "\t");
	reject("\\\n", " ");

	match("\\\t", "\t");
	reject("\\\t", "\n");
	reject("\\\t", " ");

	match("\\ ", " ");
	reject("\\ ", "\n");
	reject("\\ ", "\t");

	reject("a\\{2,2\\}", "aa");
	match("a\\{2,2\\}", "a{2,2}");
	reject("a\\{2,2\\}", "a\\{2,2\\}");

	reject("a\\+", "a");
	reject("a\\+", "aa");
	match("a\\+", "a+");
	reject("a\\+", "a\\+");

	reject("\\(a\\)", "a");
	match("\\(a\\)", "(a)");
	reject("\\(a\\)", "\\(a\\)");

	reject("a\\|b", "a");
	reject("a\\|b", "b");
	match("a\\|b", "a|b");
	reject("a\\|b", "a\\|b");

	return 0;
}
