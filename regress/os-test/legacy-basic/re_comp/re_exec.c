/*[SUSv2]*/
/* Test whether re_comp compiles and re_exec matches. */

#include <re_comp.h>
#include <stddef.h>

#include "../basic.h"

int main(void)
{
	if ( re_exec("x") != -1 )
		errx(1, "re_exec before any re_comp was not -1");
	if ( re_comp("") == NULL )
		errx(1, "re_comp(\"\") with no previous expression did not fail");
	if ( re_comp("a.c") != NULL )
		errx(1, "re_comp(\"a.c\") failed");
	if ( re_exec("xxabcxx") != 1 )
		errx(1, "a.c did not match xxabcxx");
	if ( re_exec("a\nc") != 0 )
		errx(1, "a.c matched a newline");
	if ( re_comp(NULL) != NULL || re_exec("abc") != 1 )
		errx(1, "re_comp(NULL) changed the expression");
	if ( re_comp("^\\(.*\\)\\1$") != NULL )
		errx(1, "re_comp of a back-reference failed");
	if ( re_exec("abcabc") != 1 || re_exec("abcabd") != 0 )
		errx(1, "^\\(.*\\)\\1$ did not tell a repeated string");
	if ( re_comp("") != NULL || re_exec("xyzxyz") != 1 )
		errx(1, "re_comp(\"\") did not keep the last expression");
	if ( re_comp("\\(") == NULL )
		errx(1, "re_comp of a bad expression did not give a message");
	return 0;
}
