/*
 * makefs: the NetBSD library functions its sources call that V86BSD's libc
 * does not have (emalloc and friends and strsuftoll are NetBSD's libc and
 * libutil, strsep is the BSDs'); written for V86BSD after their manual pages.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nbtool_config.h"

/* what kind of file a mode is (inotype() of NetBSD's mtree) */
const char *
inotype(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFBLK:
		return "block";
	case S_IFCHR:
		return "char";
	case S_IFDIR:
		return "dir";
	case S_IFIFO:
		return "fifo";
	case S_IFREG:
		return "file";
	case S_IFLNK:
		return "link";
	case S_IFSOCK:
		return "socket";
	default:
		return "unknown";
	}
}

void *
emalloc(size_t n)
{
	void *p = malloc(n);

	if (p == NULL)
		err(1, "malloc");
	return p;
}

void *
ecalloc(size_t n, size_t m)
{
	void *p = calloc(n, m);

	if (p == NULL)
		err(1, "calloc");
	return p;
}

void *
erealloc(void *q, size_t n)
{
	void *p = realloc(q, n);

	if (p == NULL)
		err(1, "realloc");
	return p;
}

char *
estrdup(const char *s)
{
	char *p = strdup(s);

	if (p == NULL)
		err(1, "strdup");
	return p;
}

char *
strsep(char **stringp, const char *delim)
{
	char *s = *stringp, *p;

	if (s == NULL)
		return NULL;
	p = s + strcspn(s, delim);
	if (*p) {
		*p++ = '\0';
		*stringp = p;
	} else
		*stringp = NULL;
	return s;
}

/*
 * A number with an optional suffix: b (512), k, m, g, t (powers of 1024), w
 * (4), and x for a product, as 2x3k; within min and max, else an error
 * message with `desc' and exit (strsuftoll(3) of NetBSD).
 */
long long
strsuftoll(const char *desc, const char *val, long long min, long long max)
{
	long long result = 0, n, mul;
	const char *p = val;
	char *end;
	int first = 1;

	for (;;) {
		errno = 0;
		n = strtoll(p, &end, 10);
		if (end == p || errno == ERANGE)
			errx(1, "%s `%s' is not a number or out of range", desc, val);
		mul = 1;
		switch (tolower((unsigned char)*end)) {
		case 'b': mul = 512; end++; break;
		case 'k': mul = 1LL << 10; end++; break;
		case 'm': mul = 1LL << 20; end++; break;
		case 'g': mul = 1LL << 30; end++; break;
		case 't': mul = 1LL << 40; end++; break;
		case 'w': mul = 4; end++; break;
		default: break;
		}
		if (n != 0 && (n > LLONG_MAX / mul || n < LLONG_MIN / mul))
			errx(1, "%s `%s' is out of range", desc, val);
		n *= mul;
		if (first) {
			result = n;
			first = 0;
		} else {
			if (result != 0 && (n > LLONG_MAX / result))
				errx(1, "%s `%s' is out of range", desc, val);
			result *= n;
		}
		if (*end == 'x') {
			p = end + 1;
			continue;
		}
		if (*end != '\0')
			errx(1, "%s `%s' has an invalid suffix", desc, val);
		break;
	}
	if (result < min)
		errx(1, "%s %lld is less than %lld", desc, result, min);
	if (result > max)
		errx(1, "%s %lld is greater than %lld", desc, result, max);
	return result;
}
