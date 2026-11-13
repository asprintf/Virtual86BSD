/* Test whether periods and slashes work properly. */

#ifndef FLAGS
#define FLAGS 0
#endif

#include "suite.h"

// The exhaustive truth table combining all the features for the purpose of
// testing the period and pathname flags is too large to hardcode in the tests,
// and it wouldn't really be maintainable and readable. Instead this test
// dynamtically generates the truth table with all short patterns and inputs
// using the relevant features.
void recurse(int depth, bool judgement, const char* pattern, const char* input,
             bool leading_dot)
{
	if ( !judgement )
	{
		reject(pattern, input);
		return;
	}
	else if ( !depth )
	{
		match(pattern, input);
		return;
	}

	char next_pattern[256];
	char next_input[256];

	recurse(depth - 1, judgement, pattern, input, leading_dot);

	// Test a period next in the pattern.
	snprintf(next_pattern, 256, "%s.", pattern);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, judgement, next_pattern, next_input, false);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, false, next_pattern, next_input, false);

	// Test a slash next in the pattern.
	snprintf(next_pattern, 256, "%s/", pattern);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, judgement, next_pattern, next_input, flags & FNM_PATHNAME);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, false, next_pattern, next_input, false);
	if ( !strchr(pattern, '*') )
	{
		snprintf(next_input, 256, "%s//", input);
		recurse(depth - 1, false, next_pattern, next_input, flags & FNM_PATHNAME);
	}

	// Test a literal x next in the pattern.
	snprintf(next_pattern, 256, "%sx", pattern);
	snprintf(next_input, 256, "%sx", input);
	recurse(depth - 1, judgement, next_pattern, next_input, false);

	// Test an escaped period next in the pattern. This should work normally.
	snprintf(next_pattern, 256, "%s\\.", pattern);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, judgement, next_pattern, next_input, false);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, false, next_pattern, next_input, false);

	// Test an escaped slash next in the pattern. This should work normally.
	snprintf(next_pattern, 256, "%s\\/", pattern);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, judgement, next_pattern, next_input, flags & FNM_PATHNAME);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, false, next_pattern, next_input, false);

	// Test a question mark next in the pattern.
	snprintf(next_pattern, 256, "%s?", pattern);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, judgement && !((flags & FNM_PERIOD) && leading_dot), next_pattern, next_input, false);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, judgement && !(flags & FNM_PATHNAME), next_pattern, next_input, flags & FNM_PATHNAME);
	snprintf(next_input, 256, "%sx", input);
	recurse(depth - 1, judgement, next_pattern, next_input, false);
	if ( !strchr(pattern, '*') )
	{
		snprintf(next_input, 256, "%s//", input);
		recurse(depth - 1, false, next_pattern, next_input, flags & FNM_PATHNAME);
	}

	// Test an asterisk next in the pattern.
	snprintf(next_pattern, 256, "%s*", pattern);
	snprintf(next_input, 256, "%s.", input);
	recurse(depth - 1, judgement && !((flags & FNM_PERIOD) && leading_dot), next_pattern, next_input, false);
	snprintf(next_input, 256, "%s/", input);
	recurse(depth - 1, judgement && !(flags & FNM_PATHNAME), next_pattern, next_input, flags & FNM_PATHNAME);
	snprintf(next_input, 256, "%sx", input);
	recurse(depth - 1, judgement, next_pattern, next_input, false);
	snprintf(next_input, 256, "%s//", input);
	recurse(depth - 1, judgement && !(flags & FNM_PATHNAME), next_pattern, next_input, flags & FNM_PATHNAME);

	const char* fragments[] =
	{
		// Many systems fail this pattern on FMN_PATHNAME because they naively
		// split on / characters, but fail to properly recognize the boundaries
		// of literal / characters and incorrectly split inside bracket
		// expressions. There are no such POSIX requirements forbidding slashes
		// inside bracket expressions on FMN_PATHNAME. Likewise for FMN_PERIOD
		// and periods, but most systems handle that case correctly.
		"[./]",
		"[,-0]",
		"[[...][./.]]",
		"[[=.=][=/=]]",
		"[[:punct:]]",
		NULL
	};

	// Test a matching bracket expression next in the pattern.
	for ( size_t i = 0; fragments[i]; i++ )
	{
		snprintf(next_pattern, 256, "%s%s", pattern, fragments[i]);
		snprintf(next_input, 256, "%s.", input);
		// POSIX-1.2024 2.14.3 2. "It is unspecified whether an explicit
		// <period> in a bracket expression matching list, such as "[.abc]",
		// can match a leading <period> in a filename."
		// These cases are studied in periodslash-period-ns.c and
		// periodslash-period-pathname-ns.c.
		if ( strcmp(fragments[i], "[./]") != 0 ||
		    !(flags & FNM_PERIOD) )
			recurse(depth - 1, judgement && !((flags & FNM_PERIOD) && leading_dot), next_pattern, next_input, false);
		snprintf(next_input, 256, "%s/", input);
		recurse(depth - 1, judgement && !(flags & FNM_PATHNAME), next_pattern, next_input, flags & FNM_PATHNAME);
		snprintf(next_input, 256, "%sx", input);
		recurse(depth - 1, false, next_pattern, next_input, false);
		if ( !strchr(pattern, '*') )
		{
			snprintf(next_input, 256, "%s//", input);
			recurse(depth - 1, false, next_pattern, next_input, flags & FNM_PATHNAME);
		}
	}

	// Test a non-matching bracket expression next in the pattern.
	for ( size_t i = 0; fragments[i]; i++ )
	{
		snprintf(next_pattern, 256, "%s[!%s", pattern, fragments[i] + 1);
		snprintf(next_input, 256, "%s.", input);
		recurse(depth - 1, false, next_pattern, next_input, false);
		snprintf(next_input, 256, "%s/", input);
		recurse(depth - 1, false, next_pattern, next_input, flags & FNM_PATHNAME);
		snprintf(next_input, 256, "%sx", input);
		recurse(depth - 1, judgement, next_pattern, next_input, false);
		if ( !strchr(pattern, '*') )
		{
			snprintf(next_input, 256, "%s//", input);
			recurse(depth - 1, false, next_pattern, next_input, flags & FNM_PATHNAME);
		}
	}
}

int main(void)
{
	flags = FLAGS;

	recurse(3, true, "", "", true);

	return 0;
}
