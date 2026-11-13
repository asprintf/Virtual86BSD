/*[SUSv2]*/
/* Test whether compile and step find simple regular expressions and set loc1 and loc2. */

#include <string.h>

static int error;
#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) ((void) (c), (void) --sp)
#define RETURN(c) return (c);
#define ERROR(c) return (error = (c), (char *) 0);
#include <regexp.h>

#include "../basic.h"

static char expbuf[1024];

static void match(const char* re, const char* s, int at, int len)
{
	error = 0;
	char pat[256];
	strcpy(pat, re);
	if ( !compile(pat, expbuf, expbuf + sizeof expbuf, '\0') )
		errx(1, "compile(\"%s\") failed: %d", re, error);
	int r = step(s, expbuf);
	if ( at < 0 )
	{
		if ( r )
			errx(1, "\"%s\" matched \"%s\"", re, s);
		return;
	}
	if ( !r )
		errx(1, "\"%s\" did not match \"%s\"", re, s);
	if ( loc1 != s + at || loc2 != s + at + len )
		errx(1, "\"%s\" in \"%s\": loc1 %d, loc2 %d, not %d, %d",
		     re, s, (int)(loc1 - s), (int)(loc2 - s), at, at + len);
}

int main(void)
{
	match("abc", "xxabcxx", 2, 3);
	match("ab*c", "xacx", 1, 2);
	match("ab*c", "xabbbcx", 1, 5);
	match("a.c", "a\nc", 0, 3);
	match("^abc", "xabc", -1, 0);
	match("^abc", "abcx", 0, 3);
	match("abc$", "abcx", -1, 0);
	match("abc$", "xabc", 1, 3);
	match("a\\{2,3\\}", "caaaab", 1, 3);
	match("a\\{2\\}b", "aaab", 1, 3);
	match("x\\{2,\\}", "axxxxb", 1, 4);
	match("\\(ab\\)c\\1", "zabcabz", 1, 5);
	match("[[:digit:]][[:digit:]]*", "ab123c", 2, 3);
	match("[^a-c]", "abcd", 3, 1);
	match("[]x]", "a]", 1, 1);
	match("a\\.b", "axb a.b", 4, 3);
	return 0;
}
