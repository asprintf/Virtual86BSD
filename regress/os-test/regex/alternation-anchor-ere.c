/* Test whether ERE alternations work combined with anchors. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("^a|b", "");
	match("^a|b", "a");
	match("^a|b", "b");
	reject("^a|b", "c");
	partial("^a|b", "aa", m(0, 1));
	partial("^a|b", "ab", m(0, 1));
	partial("^a|b", "ac", m(0, 1));
	partial("^a|b", "ba", m(0, 1));
	partial("^a|b", "bb", m(0, 1));
	partial("^a|b", "bc", m(0, 1));
	reject("^a|b", "ca");
	partial("^a|b", "cb", m(1, 2));
	reject("^a|b", "cc");

	reject("a$|b", "");
	match("a$|b", "a");
	match("a$|b", "b");
	reject("a$|b", "c");
	partial("a$|b", "aa", m(1, 2));
	partial("a$|b", "ab", m(1, 2));
	reject("a$|b", "ac");
	partial("a$|b", "ba", m(0, 1));
	partial("a$|b", "bb", m(0, 1));
	partial("a$|b", "bc", m(0, 1));
	partial("a$|b", "ca", m(1, 2));
	partial("a$|b", "cb", m(1, 2));
	reject("a$|b", "cc");

	reject("a|^b$", "");
	match("a|^b$", "a");
	match("a|^b$", "b");
	reject("a|^b$", "c");
	partial("a|^b$", "aa", m(0, 1));
	partial("a|^b$", "ab", m(0, 1));
	partial("a|^b$", "ab", m(0, 1));
	partial("a|^b$", "ba", m(1, 2));
	reject("a|^b$", "bb");
	reject("a|^b$", "bc");
	partial("a|^b$", "ca", m(1, 2));
	reject("a|^b$", "cb");
	reject("a|^b$", "cc");

	return 0;
}

