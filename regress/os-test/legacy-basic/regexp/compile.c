/*[SUSv2]*/
/* Test whether compile reports the errors SUSv2 numbers. */

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

static void bad(const char* re, int eof, int want, size_t size)
{
	static char expbuf[1024];
	char pat[256];
	strcpy(pat, re);
	error = 0;
	if ( compile(pat, expbuf, expbuf + size, eof) )
		errx(1, "compile(\"%s\") succeeded", re);
	if ( error != want )
		errx(1, "compile(\"%s\") gave error %d, not %d", re, error, want);
}

int main(void)
{
	static char fresh[64];
	char empty[] = "";
	error = 0;
	if ( compile(empty, fresh, fresh + sizeof fresh, '\0') || error != 41 )
		errx(1, "an empty pattern with none before was not error 41");
	bad("a\\{256\\}", '\0', 11, 1024);
	bad("a\\{x\\}", '\0', 16, 1024);
	bad("\\(a\\)\\2", '\0', 25, 1024);
	bad("abc", '/', 36, 1024);
	bad("\\(a", '\0', 42, 1024);
	bad("\\(\\(\\(\\(\\(\\(\\(\\(\\(\\(a\\)\\)\\)\\)\\)\\)\\)\\)\\)\\)", '\0', 43, 1024);
	bad("a\\{1,2,3\\}", '\0', 44, 1024);
	bad("a\\{1,2", '\0', 45, 1024);
	bad("a\\{3,2\\}", '\0', 46, 1024);
	bad("[abc", '\0', 49, 1024);
	bad("abcdefghijklmnopqrstuvwxyz", '\0', 50, 16);
	return 0;
}
