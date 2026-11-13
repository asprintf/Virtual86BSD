/* Test whether bracket character class elements work with backslashes in the
   name of the character class. */

#include "suite.h"

int main(void)
{
	flags = 0;

	// Control tests to verify the base feature works.
	match("[[:alnum:]]", "a");
	reject("[[:alnum:]]", "#");

	reject("[![:alnum:]]", "a");
	match("[![:alnum:]]", "#");

	// os-test wishes to express its apologies to everyone.
	//
	// I don't think anybody ever considered this case and it works on no
	// existing operating system. However POSIX.1-2024 2.14.1 is quite clear:
	//
	// "In a pattern, or part of one, where a shell-quoting <backslash> can be
	//  used, a <backslash> character shall escape the following character as
	//  described in 2.2.1 Escape Character (Backslash), regardless of whether
	//  or not the <backslash> is inside a bracket expression. "
	//
	// POSIX expressly clarifies patterns like [\x] [\]] do escape in its
	// examples. There are no special exemptions defined for the [[:foo:]],
	// [[=x=]] and [[.x.]] syntaxes with regards to backslashes. fnmatch is
	// supposed to implement shell escaping throughout the whole string, where
	// \x means a literal x for all characters x. For those reasons, it follows
	// that [[:al\num:]] must be equivalent to [[:alnum:]]. Unless you tried
	// to interpret that alnum has a special meaning and \ somehow changes that
	// to another meaning, but that isn't the case, is it? A backslash in the
	// [: sequence such as [\[:x:]] and [[\:x:]] does restore the original
	// literal meaning, so [: is certainly special, but I see no argument that
	// alnum is special.
	//
	// With all of that reasoning in mind, I have to conclude that [[:al\num:]]
	// must be equivalent to [[:alnum:]]. These semantics sort of suck because
	// now fnmatch needs to use a special kind of strncmp to look up the
	// character classes that takes backslashes into account. On the bright side
	// shell escaping works everywhere and that is also a technical quality. It
	// is fairly pointless for character classes since no name needs to be
	// escaped, but it can matter for collation elements and equivalence
	// classes.
	//
	// We may need an official POSIX resolution here.
	match("[[:al\\num:]]", "a");
	reject("[[:al\\num:]]", "#");

	reject("[![:al\\num:]]", "a");
	match("[![:al\\num:]]", "#");

	return 0;
}
