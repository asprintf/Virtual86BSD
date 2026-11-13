/* Test whether BRE subexpressions works. */

#include "suite.h"

int main(void)
{
	cflags = 0;
	matches("\\(a\\)", "a", 2, (M[]) { m(0, 1), m(0, 1) });
	reject("\\(a\\)", "b");
	matches("\\(b\\)", "abc", 2, (M[]) { m(1, 2), m(1, 2) });
	matches("\\(b\\)\\(c\\)", "abc", 3, (M[]) { m(1, 3), m(1, 2), m(2, 3) });
	matches("\\(ab\\(cd\\)ef\\)", "abcdef", 3, (M[]) { m(0, 6), m(0, 6), m(2, 4) });
	matches("a\\(b\\(cd\\)e\\)f", "abcdef", 3, (M[]) { m(0, 6), m(1, 5), m(2, 4) });
	matches("a\\(aa\\)\\(aa\\)a", "aaaaaa", 3, (M[]) { m(0, 6), m(1, 3), m(3, 5) });
	reject("a\\(aa\\)\\(aa\\)a", "abcdef");
	matches("a\\(bc\\)\\(de\\)f", "abcdef", 3, (M[]) { m(0, 6), m(1, 3), m(3, 5) });
	reject("a\\(bc\\)\\(de\\)f", "aaaaaa");
	matches("a\\(b\\(c\\)d\\)\\(e\\(f\\)g\\)h", "abcdefgh", 5, (M[]) { m(0, 8), m(1, 4), m(2, 3), m(4, 7), m(5, 6) });
	matches("a\\(b\\(c\\)d\\)\\(e\\(f\\)g\\)h", "abcdefghxyz", 5, (M[]) { m(0, 8), m(1, 4), m(2, 3), m(4, 7), m(5, 6) });
	reject("a\\(b\\(c\\)d\\)\\(e\\(f\\)g\\)h", "abcdefgx");
	matches("a\\(b\\(c\\(d\\)\\)\\)", "abcd", 4, (M[]) { m(0, 4), m(1, 4), m(2, 4), m(3, 4) });
	matches("\\(\\(\\(a\\)b\\)c\\)d", "abcd", 4, (M[]) { m(0, 4), m(0, 3), m(0, 2), m(0, 1) });
	return 0;
}
