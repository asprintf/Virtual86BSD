/* Test whether backslashes work with basic characters in noescape mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_NOESCAPE;

	reject("\\\\", "");
	reject("\\\\", "\\");
	reject("\\\\", "a");
	match("\\\\", "\\\\");

	reject("\\a", "");
	reject("\\a", "a");
	reject("\\a", "b");
	reject("\\a", "aa");
	reject("\\a", "\\a\\b");
	reject("\\a", "ba");
	reject("\\a", "bb");
	match("\\a", "\\a");

	reject("\\a\\b", "");
	reject("\\a\\b", "a");
	reject("\\a\\b", "b");
	reject("\\a\\b", "aa");
	reject("\\a\\b", "ab");
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
	match("\\a\\b", "\\a\\b");

	reject("\\/", "");
	reject("\\/", "/");
	match("\\/", "\\/");
	reject("\\/", "#");

	reject("\\.", "");
	reject("\\.", ".");
	match("\\.", "\\.");
	reject("\\.", "a");

	reject("\\{", "{");
	match("\\{", "\\{");
	reject("\\{", "a");

	reject("\\}", "}");
	match("\\}", "\\}");
	reject("\\}", "a");

	reject("\\(", "(");
	match("\\(", "\\(");
	reject("\\(", "a");

	reject("\\)", ")");
	match("\\)", "\\)");
	reject("\\)", "a");

	reject("\\!", "!");
	match("\\!", "\\!");
	reject("\\!", "a");

	reject("\\^", "^");
	match("\\^", "\\^");
	reject("\\^", "a");

	reject("\\]", "]");
	match("\\]", "\\]");
	reject("\\]", "a");

	reject("\\|", "|");
	match("\\|", "\\|");
	reject("\\|", "a");

	reject("\\\n", "\n");
	reject("\\\n", "\t");
	reject("\\\n", " ");
	match("\\\n", "\\\n");
	reject("\\\n", "\\\t");
	reject("\\\n", "\\ ");

	reject("\\\t", "\t");
	reject("\\\t", "\n");
	reject("\\\t", " ");
	match("\\\t", "\\\t");
	reject("\\\t", "\\\n");
	reject("\\\t", "\\ ");

	reject("\\ ", " ");
	reject("\\ ", "\n");
	reject("\\ ", "\t");
	match("\\ ", "\\ ");
	reject("\\ ", "\\n");
	reject("\\ ", "\\t");

	reject("a\\{2,2\\}", "aa");
	reject("a\\{2,2\\}", "a{2,2}");
	match("a\\{2,2\\}", "a\\{2,2\\}");

	reject("a\\+", "a");
	reject("a\\+", "aa");
	reject("a\\+", "a+");
	match("a\\+", "a\\+");

	reject("\\(a\\)", "a");
	reject("\\(a\\)", "(a)");
	match("\\(a\\)", "\\(a\\)");

	reject("a\\|b", "a");
	reject("a\\|b", "b");
	reject("a\\|b", "a|b");
	match("a\\|b", "a\\|b");

	return 0;
}
