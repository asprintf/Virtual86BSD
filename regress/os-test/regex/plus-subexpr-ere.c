/* Test whether ERE pluses work combined with subexpressions. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("(a+)(b+)", "");
	reject("(a+)(b+)", "a");
	reject("(a+)(b+)", "b");
	reject("(a+)(b+)", "aa");
	matches("(a+)(b+)", "ab", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	reject("(a+)(b+)", "ba");
	reject("(a+)(b+)", "bb");

	reject("(a)+(b)+", "");
	reject("(a)+(b)+", "a");
	reject("(a)+(b)+", "b");
	reject("(a)+(b)+", "aa");
	matches("(a)+(b)+", "ab", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	reject("(a)+(b)+", "ba");
	reject("(a)+(b)+", "bb");

	reject("(a+)(a+)", "");
	reject("(a+)(a+)", "a");
	matches("(a+)(a+)", "aa", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	matches("(a+)(a+)", "aaa", 3, (M[]) { m(0, 3), m(0, 2), m(2, 3) });
	matches("(a+)(a+)", "aaaa", 3, (M[]) { m(0, 4), m(0, 3), m(3, 4) });

	reject("(a)+(a)+", "");
	reject("(a)+(a)+", "a");
	matches("(a)+(a)+", "aa", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	matches("(a)+(a)+", "aaa", 3, (M[]) { m(0, 3), m(1, 2), m(2, 3) });
	matches("(a)+(a)+", "aaaa", 3, (M[]) { m(0, 4), m(2, 3), m(3, 4) });

	reject("(ab)+", "");
	reject("(ab)+", "a");
	matches("(ab)+", "ab", 2, (M[]) { m(0, 2), m(0, 2) });
	matches("(ab)+", "aba", 2, (M[]) { m(0, 2), m(0, 2) });
	matches("(ab)+", "abab", 2, (M[]) { m(0, 4), m(2, 4) });

	reject("(a((a+)+))", "");
	reject("(a((a+)+))", "a");
	matches("(a((a+)+))", "aa", 4, (M[]) { m(0, 2), m(0, 2), m(1, 2), m(1, 2) });
	matches("(a((a+)+))", "aaa", 4, (M[]) { m(0, 3), m(0, 3), m(1, 3), m(1, 3) });
	matches("(a((a+)+))", "aaaa", 4, (M[]) { m(0, 4), m(0, 4), m(1, 4), m(1, 4) });

	reject("(a)+ab", "");
	reject("(a)+ab", "a");
	reject("(a)+ab", "b");
	reject("(a)+ab", "aa");
	reject("(a)+ab", "ab");
	matches("(a)+ab", "aab", 2, (M[]) { m(0, 3), m(0, 1) });
	matches("(a)+ab", "aaab", 2, (M[]) { m(0, 4), m(1, 2) });

	reject("(a+)ab", "");
	reject("(a+)ab", "a");
	reject("(a+)ab", "b");
	reject("(a+)ab", "aa");
	reject("(a+)ab", "ab");
	matches("(a+)ab", "aab", 2, (M[]) { m(0, 3), m(0, 1) });
	matches("(a+)ab", "aaab", 2, (M[]) { m(0, 4), m(0, 2) });

	return 0;
}
