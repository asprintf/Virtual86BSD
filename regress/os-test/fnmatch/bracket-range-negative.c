/* Test whether brackets ranges work when in negative order. */

#include "suite.h"

int main(void)
{
	// POSIX.1-2024 9.3.5 7. "If the represented set of collating elements is
	// empty, it is unspecified whether the expression matches nothing, or is
	// treated as invalid."
	reject("[d-b]", "a");
	reject("[d-b]", "b");
	reject("[d-b]", "c");
	reject("[d-b]", "d");
	reject("[d-b]", "e");

	return 0;
}
