/* Test whether ERE alternations work combined with subexpressions. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("(a(b*))|(ba)|((b|a)(a|b))", "");
	matches("(a(b*))|(ba)|((b|a)(a|b))", "a", 7, (M[]) { m(0, 1), m(0, 1), m(1, 1), m(-1, -1), m(-1, -1), m(-1, -1), m(-1, -1) });
	reject("(a(b*))|(ba)|((b|a)(a|b))", "b");
	matches("(a(b*))|(ba)|((b|a)(a|b))", "aa", 7, (M[]) { m(0, 2), m(-1, -1), m(-1, -1), m(-1, -1), m(0, 2), m(0, 1), m(1, 2) });
	matches("(a(b*))|(ba)|((b|a)(a|b))", "ab", 7, (M[]) { m(0, 2), m(0, 2), m(1, 2), m(-1, -1), m(-1, -1), m(-1, -1), m(-1, -1) });
	matches("(a(b*))|(ba)|((b|a)(a|b))", "ba", 7, (M[]) { m(0, 2), m(-1, -1), m(-1, -1), m(0, 2), m(-1, -1), m(-1, -1), m(-1, -1) });
	matches("(a(b*))|(ba)|((b|a)(a|b))", "bb", 7, (M[]) { m(0, 2), m(-1, -1), m(-1, -1), m(-1, -1), m(0, 2), m(0, 1), m(1, 2) });

	matches("(fo|foo)(obars|bar)", "foobar", 3, (M[]) { m(0, 6), m(0, 3), m(3, 6) });
	matches("(fo|foo)(obars|bar)", "foobars", 3, (M[]) { m(0, 7), m(0, 2), m(2, 7) });

	matches("(a)|(a)", "a", 3, (M[]) { m(0, 1), m(0, 1), m(-1, -1) });

	return 0;
}
