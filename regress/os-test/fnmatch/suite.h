#include <errno.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../misc/errors.h"

static int flags = 0;

char* describe(void)
{
	static char str[256];
	str[0] = '\0';
#ifdef FNM_PATHNAME
	if ( flags & FNM_PATHNAME )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " FNM_PATHNAME");
#endif
#ifdef FNM_PERIOD
	if ( flags & FNM_PERIOD )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " FNM_PERIOD");
#endif
#ifdef FNM_NOESCAPE
	if ( flags & FNM_NOESCAPE )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " FNM_NOESCAPE");
#endif
#ifdef FNM_CASEFOLD
	if ( flags & FNM_CASEFOLD )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " FNM_CASEFOLD");
#endif
	return str;
}

void match(const char* pattern, const char* string)
{
	int result = fnmatch(pattern, string, flags);
	if ( result == FNM_NOMATCH )
		errx(1, "'%s'%s did not match \"%s\"", pattern, describe(), string);
	else if ( result != 0 )
		errx(1, "'%s'%s fnmatch returned weird result %d on \"%s\"",
		     pattern, describe(), result, string);
}

void reject(const char* pattern, const char* string)
{
	int result = fnmatch(pattern, string, flags);
	if ( result == 0 )
		errx(1, "'%s'%s did not reject \"%s\"", pattern, describe(), string);
	else if ( result != FNM_NOMATCH )
		errx(1, "'%s'%s fnmatch returned weird result %d on \"%s\"",
		     pattern, describe(), result, string);
}

#if 0
#define match(a, b) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), match(a, b))
#define reject(a, b) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), reject(a, b))
#endif
