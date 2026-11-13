/* Test whether brackets works. */

#include "suite.h"

void run(void)
{
	match("[a]", "a");
	reject("[a]", "b");
	reject("[a]", "");
	partial("[a]", "ab", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });
	partial("[a]", "aa", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });

	reject("[^a]", "a");
	match("[^a]", "b");
	reject("[^a]", "");
	partial("[^a]", "ab", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });
	reject("[^a]", "aa");
	reject("[a^]", "");
	match("[a^]", "^");
	match("[a^]", "a");
	reject("[a^]", "b");

	match("[]]", "]");
	reject("[]]", "a");
	reject("[]]", "[");
	match("[][]", "[");
	match("[][]", "]");
	reject("[][]", "a");

	reject("[^]]", "]");
	match("[^]]", "a");
	match("[^]]", "[");
	reject("[^][]", "[");
	reject("[^][]", "]");
	match("[^][]", "a");

	reject("[abc]", "");
	match("[abc]", "a");
	match("[abc]", "b");
	match("[abc]", "c");
	reject("[abc]", "d");
	reject("[abc]", "A");
	partial("[abc]", "xabcy", (regmatch_t) { .rm_so = 1, .rm_eo = 2 });

	reject("[^abc]", "");
	reject("[^abc]", "a");
	reject("[^abc]", "b");
	reject("[^abc]", "c");
	match("[^abc]", "d");
	match("[^abc]", "A");
	partial("[^abc]", "xabcy", (regmatch_t) { .rm_so = 0, .rm_eo = 1 });

	match("[.]", ".");
	reject("[.]", "a");

	match("[*]", "*");
	reject("[*]", "a");

	match("[(]", "(");
	reject("[(]", "a");

	match("[)]", ")");
	reject("[)]", "a");

	match("[+]", "+");
	reject("[+]", "a");

	match("[?]", "?");
	reject("[?]", "a");

	match("[{]", "{");
	reject("[{]", "a");

	match("[}]", "}");
	reject("[}]", "a");

	match("[|]", "|");
	reject("[|]", "a");

	match("[$]", "$");
	reject("[$]", "a");

	match("[$]", "$");
	reject("[$]", "a");

	reject("[^^]", "");
	reject("[^^]", "^");
	match("[^^]", "a");

	reject("[^a[b^]", "");
	reject("[^a[b^]", "a");
	reject("[^a[b^]", "[");
	reject("[^a[b^]", "b");
	reject("[^a[b^]", "^");
	match("[^a[b^]", "c");

	match("[!x]", "x");
	match("[!x]", "!");

	reject("[!x]", "y");
	reject("[!x]", "^");

	match("[\\]", "\\");
	reject("[\\]", "]");
	reject("[\\]", "x");

	match("[\\\\]", "\\");
	reject("[\\\\]", "]");
	reject("[\\\\]", "x");

	match("[x\\y]", "\\");
	reject("[x\\y]", "]");
	match("[x\\y]", "x");
	match("[x\\y]", "y");
	reject("[x\\y]", "z");

	match("[][\\]", "\\");
	match("[][\\]", "]");
	match("[][\\]", "[");
	reject("[][\\]", "x");

	reject("[][x\\]]", "");
	reject("[][x\\]]", "x");
	reject("[][x\\]]", "\\");
	reject("[][x\\]]", "]");
	reject("[][x\\]]", "[");
	reject("[][x\\]]", "y");
	match("[][x\\]]", "x]");
	match("[][x\\]]", "\\]");
	match("[][x\\]]", "]]");
	match("[][x\\]]", "[]");

	reject("[^][x\\]]", "");
	reject("[^][x\\]]", "x");
	reject("[^][x\\]]", "\\");
	reject("[^][x\\]]", "]");
	reject("[^][x\\]]", "[");
	reject("[^][x\\]]", "y");
	reject("[^][x\\]]", "x]");
	reject("[^][x\\]]", "\\]");
	reject("[^][x\\]]", "]]");
	reject("[^][x\\]]", "[]");
	match("[^][x\\]]", "y]");
}

int main(void)
{
	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
