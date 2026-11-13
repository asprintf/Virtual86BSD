/* Test whether ERE intervals work. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("a{1}", "");
	match("a{1}", "a");
	partial("a{1}", "aa", m(0, 1));

	reject("a{2}", "");
	reject("a{2}", "a");
	match("a{2}", "aa");

	reject("a{2,5}", "");
	reject("a{2,5}", "a");
	match("a{2,5}", "aa");
	match("a{2,5}", "aaa");
	match("a{2,5}", "aaaa");
	match("a{2,5}", "aaaaa");
	partial("a{2,5}", "aaaaaa", m(0, 5));

	reject("a{2,}", "");
	reject("a{2,}", "a");
	match("a{2,}", "aa");
	match("a{2,}", "aaa");
	match("a{2,}", "aaaa");
	match("a{2,}", "aaaaa");
	match("a{2,}", "aaaaaa");

	match("a{0,0}", "");
	partial("a{0,0}", "a", m(0, 0));
	partial("a{0,0}", "aa", m(0, 0));

	match("a{0}", "");
	partial("a{0}", "a", m(0, 0));
	partial("a{0}", "aa", m(0, 0));

	match("a{0,5}", "");
	match("a{0,5}", "aaaaa");
	partial("a{0,5}", "aaaaaa", m(0, 5));
	partial("a{0,5}", "xaaaaa", m(0, 0));

	return 0;
}
