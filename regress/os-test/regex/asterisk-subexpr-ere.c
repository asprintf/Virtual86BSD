/* Test whether ERE asterisks work combined with subexpressions. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	matches("(a*)(b*)", "", 3, (M[]) { m(0, 0), m(0, 0), m(0, 0) });
	matches("(a*)(b*)", "a", 3, (M[]) { m(0, 1), m(0, 1), m(1, 1) });
	matches("(a*)(b*)", "b", 3, (M[]) { m(0, 1), m(0, 0), m(0, 1) });
	matches("(a*)(b*)", "aa", 3, (M[]) { m(0, 2), m(0, 2), m(2, 2) });
	matches("(a*)(b*)", "ab", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	matches("(a*)(b*)", "ba", 3, (M[]) { m(0, 1), m(0, 0), m(0, 1) });
	matches("(a*)(b*)", "bb", 3, (M[]) { m(0, 2), m(0, 0), m(0, 2) });

	matches("(a)*(b)*", "", 3, (M[]) { m(0, 0), m(-1, -1), m(-1, -1) });
	matches("(a)*(b)*", "a", 3, (M[]) { m(0, 1), m(0, 1), m(-1, -1) });
	matches("(a)*(b)*", "b", 3, (M[]) { m(0, 1), m(-1, -1), m(0, 1) });
	matches("(a)*(b)*", "aa", 3, (M[]) { m(0, 2), m(1, 2), m(-1, -1) });
	matches("(a)*(b)*", "ab", 3, (M[]) { m(0, 2), m(0, 1), m(1, 2) });
	matches("(a)*(b)*", "ba", 3, (M[]) { m(0, 1), m(-1, -1), m(0, 1) });
	matches("(a)*(b)*", "bb", 3, (M[]) { m(0, 2), m(-1, -1), m(1, 2) });

	matches("(a*)(a*)", "", 3, (M[]) { m(0, 0), m(0, 0), m(0, 0) });
	matches("(a*)(a*)", "a", 3, (M[]) { m(0, 1), m(0, 1), m(1, 1) });
	matches("(a*)(a*)", "aa", 3, (M[]) { m(0, 2), m(0, 2), m(2, 2) });

	matches("(a)*(a)*", "", 3, (M[]) { m(0, 0), m(-1, -1), m(-1, -1) });
	matches("(a)*(a)*", "a", 3, (M[]) { m(0, 1), m(0, 1), m(-1, -1) });
	matches("(a)*(a)*", "aa", 3, (M[]) { m(0, 2), m(1, 2), m(-1, -1) });

	matches("(ab)*", "", 2, (M[]) { m(0, 0), m(-1, -1) });
	matches("(ab)*", "a", 2, (M[]) { m(0, 0), m(-1, -1) });
	matches("(ab)*", "ab", 2, (M[]) { m(0, 2), m(0, 2) });
	matches("(ab)*", "aba", 2, (M[]) { m(0, 2), m(0, 2) });
	matches("(ab)*", "abab", 2, (M[]) { m(0, 4), m(2, 4) });

	reject("(a((a*)*))", "");
	matches("(a((a*)*))", "a", 4, (M[]) { m(0, 1), m(0, 1), m(1, 1), m(1, 1) });
	matches("(a((a*)*))", "aa", 4, (M[]) { m(0, 2), m(0, 2), m(1, 2), m(1, 2) });
	matches("(a((a*)*))", "aaa", 4, (M[]) { m(0, 3), m(0, 3), m(1, 3), m(1, 3) });
	matches("(a((a*)*))", "aaaa", 4, (M[]) { m(0, 4), m(0, 4), m(1, 4), m(1, 4) });

	reject("(a)*ab", "");
	reject("(a)*ab", "a");
	reject("(a)*ab", "b");
	reject("(a)*ab", "aa");
	matches("(a)*ab", "ab", 2, (M[]) { m(0, 2), m(-1, -1) });
	matches("(a)*ab", "aab", 2, (M[]) { m(0, 3), m(0, 1) });
	matches("(a)*ab", "aaab", 2, (M[]) { m(0, 4), m(1, 2) });

	reject("(a*)ab", "");
	reject("(a*)ab", "a");
	reject("(a*)ab", "b");
	reject("(a*)ab", "aa");
	matches("(a*)ab", "ab", 2, (M[]) { m(0, 2), m(0, 0) });
	matches("(a*)ab", "aab", 2, (M[]) { m(0, 3), m(0, 1) });
	matches("(a*)ab", "aaab", 2, (M[]) { m(0, 4), m(0, 2) });

	return 0;
}
