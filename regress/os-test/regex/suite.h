#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../misc/errors.h"

// Shorthands to increase test expectation density. POSIX does not define the
// order of rm_so and rm_eo, so a compound literal with designated initializers
// are required.
#define M regmatch_t
#define m(so, eo) (regmatch_t) { .rm_so = (so), .rm_eo = (eo) }

static int cflags = 0;
static int eflags = 0;

int test(const char* pattern, const char* string, size_t num_matches,
         regmatch_t* matches)
{
	const char* type = cflags & REG_EXTENDED ? "ERE" : "BRE";
	int ret;
	regex_t re;
	if ( (ret = regcomp(&re, pattern, cflags)) )
	{
		char msg[256];
		regerror(ret, &re, msg, sizeof(msg));
		errx(1, "%s regcomp: %s: %s", type, msg, pattern);
	}
	if ( num_matches && re.re_nsub != num_matches - 1 )
		errx(1, "%s '%s' had %zu subexpressions instead of %zu",
		     type, pattern, re.re_nsub, num_matches - 1);
	if ( (ret = regexec(&re, string, num_matches, matches, eflags)) &&
	     ret != REG_NOMATCH )
	{
		char msg[256];
		regerror(ret, &re, msg, sizeof(msg));
		errx(1, "%s regexec: %s: %s: %s", type, msg, pattern, string);
	}
	regfree(&re);
	return ret;
}

char* describe(void)
{
	static char str[256];
	const char* type = cflags & REG_EXTENDED ? "ERE" : "BRE";
	snprintf(str, sizeof(str), "%s", type);
#ifdef REG_ICASE
	if ( cflags & REG_ICASE )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " REG_ICASE");
#endif
#ifdef REG_MINIMAL
	if ( cflags & REG_MINIMAL )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " REG_MINIMAL");
#endif
#ifdef REG_NEWLINE
	if ( cflags & REG_NEWLINE )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " REG_NEWLINE");
#endif
#ifdef REG_NOTBOL
	if ( eflags & REG_NOTBOL )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " REG_NOTBOL");
#endif
#ifdef REG_NOTEOL
	if ( eflags & REG_NOTEOL )
		snprintf(str + strlen(str), sizeof(str) - strlen(str), " REG_NOTEOL");
#endif
	return str;
}

void matches(const char* pattern, const char* string, size_t num_expectations,
             regmatch_t* expectations)
{
	regmatch_t matches[num_expectations];
	if ( test(pattern, string, num_expectations, matches) )
		errx(1, "%s '%s' did not match \"%s\"", describe(), pattern, string);
	for ( size_t i = 0; i < num_expectations; i++ )
	{
		if ( matches[i].rm_so != expectations[i].rm_so ||
		     matches[i].rm_eo != expectations[i].rm_eo )
			errx(1, "%s '%s' on \"%s\" [%zu] "
			     "matched \"%.*s\" [%zd-%zd] instead of \"%.*s\" [%zd-%zd]",
			     describe(), pattern, string, i,
			     matches[i].rm_eo - matches[i].rm_so,
			     string + matches[i].rm_so,
			     (ssize_t) matches[i].rm_so,
			     (ssize_t) matches[i].rm_eo,
			     expectations[i].rm_eo - expectations[i].rm_so,
			     string + expectations[i].rm_so,
			     (ssize_t) expectations[i].rm_so,
			     (ssize_t) expectations[i].rm_eo);
	}
}

void partial(const char* pattern, const char* string, regmatch_t match)
{
	matches(pattern, string, 1, &match);
}

void match(const char* pattern, const char* string)
{
	size_t length = strlen(string);
	regmatch_t match = { .rm_so = 0, .rm_eo = length };
	matches(pattern, string, 1, &match);
}

void reject(const char* pattern, const char* string)
{
	if ( !test(pattern, string, 0, NULL) )
		errx(1, "%s \"%s\" did not reject \"%s\"", describe(), pattern, string);
}

#if 0
#define matches(a, b, c, d) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), matches(a, b, c, d))
#define partial(a, b, c) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), partial(a, b, c))
#define match(a, b) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), match(a, b))
#define reject(a, b) (fprintf(stderr, "%s:%u\n", __FILE__, __LINE__), reject(a, b))
#endif
