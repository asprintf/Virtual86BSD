/*optional*/
/*[nonstandard]*/
/* Test how bracket character class elements work on invalid character class
   names. */

#include "suite.h"

int main(void)
{
	// Almost all systems agree that [[:invalid:]] does not match anything, but
	// it varies how the pattern fails. Some systems fail the entire pattern,
	// whereas others consider the character class empty, some even consider the
	// character class to be full, and some even return an unexpected error code
	// from fnmatch. These tests throw various invalid inputs into fnmatch and
	// studies what happens. Since the error handling is nonstandard, it is hard
	// to say that any particular behavior is wrong; although I would tend to
	// think the macOS behavior of returning 2 as an error code stands out and
	// feels wrong. The AIX behavior of matching all characters also feel
	// counter-intuitive to be and stands out. None of the systems invoke the [
	// matching [ fallback logic that is otherwise used for invalid bracket
	// expressions.
	reject("[[:invalid:]]", "[[:invalid:]]");
	reject("[[:invalid:]]", "a");

	return 0;
}
