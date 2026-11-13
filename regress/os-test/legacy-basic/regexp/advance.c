/*[SUSv2]*/
/* Test whether advance matches at the start of the string and honours locs. */

#include <string.h>

#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) ((void) (c), (void) --sp)
#define RETURN(c) return (c);
#define ERROR(c) return (void) (c), (char *) 0;
#include <regexp.h>

#include "../basic.h"

static char expbuf[1024];

int main(void)
{
	char pat[] = "a*b/";
	if ( !compile(pat, expbuf, expbuf + sizeof expbuf, '/') )
		errx(1, "compile failed");
	const char* s = "aaab rest";
	if ( !advance(s, expbuf) || loc2 != s + 4 )
		errx(1, "advance did not match the start, up to the b");
	if ( advance("xaab", expbuf) )
		errx(1, "advance matched elsewhere than at the start");
	/* backing up from the longest match stops when it reaches locs */
	char pat2[] = "a*a/";
	if ( !compile(pat2, expbuf, expbuf + sizeof expbuf, '/') )
		errx(1, "compile failed");
	const char* t = "aaa";
	locs = (char*) t + 2;
	if ( advance(t, expbuf) )
		errx(1, "advance did not stop backing up at locs");
	locs = NULL;
	if ( !advance(t, expbuf) || loc2 != t + 3 )
		errx(1, "advance without locs did not match aaa");
	return 0;
}
