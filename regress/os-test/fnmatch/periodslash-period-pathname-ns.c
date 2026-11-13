/*[nonstandard]*/
/* Test whether an explicit period in a bracket expression matches a leading
   slash in FNM_PERIOD | FNM_PATHNAME mode. */

#include "suite.h"

int main(void)
{
	flags = FNM_PERIOD | FNM_PATHNAME;

	match("[.abc]", "a");
	match("[.abc]", "b");
	match("[.abc]", "c");
	// POSIX-1.2024 2.14.3 2. "It is unspecified whether an explicit
	// <period> in a bracket expression matching list, such as "[.abc]",
	// can match a leading <period> in a filename."
	// However almost all systems reject it in practice.
	reject("[.abc]", ".");

	match("/[.abc]", "/a");
	match("/[.abc]", "/b");
	match("/[.abc]", "/c");
	// Ditto.
	reject("/[.abc]", "/.");

	return 0;
}
