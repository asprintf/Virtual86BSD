/* Test whether BRE back references work. */

#include "suite.h"

int main(void)
{
	cflags = 0;

	// Back references are mathematically not a part of regular languages /
	// regular expressions, and cannot be done efficiently in the same way as
	// actual regular expressions. Nonetheless, they are part of POSIX BRE for
	// some reason, but not ERE (the only sensible regular expression format).

	matches("\\(abc\\)\\1", "abcabc", 2, (M[]) { m(0, 6), m(0, 3) });
	reject("\\(abc\\)\\1", "abcabd");

	reject("\\(abc\\)\\1", "abc");
	matches("\\(abc\\)\\1\\(xyz\\)\\2", "abcabcxyzxyz", 3, (M[]) { m(0, 12), m(0, 3), m(6, 9) });

	matches("\\(a\\(b\\(c\\(d\\)\\)\\)\\)\\3\\3\\2", "abcdcdcdbcd", 5, (M[]) { m(0, 11), m(0, 4), m(1, 4), m(2, 4), m(3, 4) });

	// Test that \\12 is parsed as \\1 2 and not combined.
	matches("\\(a\\)\\12", "aa2", 2, (M[]) { m(0, 3), m(0, 1) });
	reject("\\(a\\)\\12", "aa3");

	// Even if there are that many subexpressions.
	matches("\\(0\\(1\\(2\\(3\\(4\\(5\\(6\\(7\\(8\\(9\\(a\\(b\\(c\\(d\\(e\\(f\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)\\11", "0123456789abcdef0123456789abcdef1", 17, (M[]) { m(0, 33), m(0, 16), m(1, 16), m(2, 16), m(3, 16), m(4, 16), m(5, 16), m(6, 16), m(7, 16), m(8, 16), m(9, 16), m(10, 16), m(11, 16), m(12, 16), m(13, 16), m(14, 16), m(15, 16) });

	return 0;
}
