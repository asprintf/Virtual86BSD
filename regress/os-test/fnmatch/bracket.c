/* Test whether brackets work. */

#include "suite.h"

int main(void)
{
	match("[a]", "a");
	reject("[a]", "b");
	reject("[a]", "");

	reject("[!a]", "a");
	match("[!a]", "b");
	reject("[!a]", "");
	reject("[!a]", "aa");
	reject("[a!]", "");
	match("[a!]", "!");
	match("[a!]", "a");
	reject("[a!]", "b");

	match("[]]", "]");
	reject("[]]", "a");
	reject("[]]", "[");
	match("[][]", "[");
	match("[][]", "]");
	reject("[][]", "a");

	reject("[!]]", "]");
	match("[!]]", "a");
	match("[!]]", "[");
	reject("[!][]", "[");
	reject("[!][]", "]");
	match("[!][]", "a");

	reject("[abc]", "");
	match("[abc]", "a");
	match("[abc]", "b");
	match("[abc]", "c");
	reject("[abc]", "d");
	reject("[abc]", "A");

	reject("[!abc]", "");
	reject("[!abc]", "a");
	reject("[!abc]", "b");
	reject("[!abc]", "c");
	match("[!abc]", "d");
	match("[!abc]", "A");

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

	reject("[!!]", "");
	reject("[!!]", "!");
	match("[!!]", "a");

	reject("[!a[b!]", "");
	reject("[!a[b!]", "a");
	reject("[!a[b!]", "[");
	reject("[!a[b!]", "b");
	reject("[!a[b!]", "!");
	match("[!a[b!]", "c");

	match("[foo]", "f");
	match("[foo]", "o");
	reject("[foo]", "fo");
	reject("[foo]", "foo");
	reject("[foo]", "oo");
	reject("[foo]", "foo]");
	reject("[foo]", "[foo");
	reject("[foo]", "[foo]");

	reject("foo]", "f");
	reject("foo]", "o");
	reject("foo]", "fo");
	reject("foo]", "foo");
	reject("foo]", "oo");
	match("foo]", "foo]");
	reject("foo]", "[foo");
	reject("foo]", "[foo]");

	reject("[:alnum:]]", "[]");
	match("[:alnum:]]", ":]");
	match("[:alnum:]]", "a]");
	match("[:alnum:]]", "l]");
	match("[:alnum:]]", "n]");
	match("[:alnum:]]", "u]");
	match("[:alnum:]]", "m]");
	reject("[:alnum:]]", "z]");

	return 0;
}
