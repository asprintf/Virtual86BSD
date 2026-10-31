#include <regex.h>
#include <string.h>

/* The messages of glibc, which scripts and tests tend to expect. */
static const char *const msgs[] = {
	"Success",
	"No match",
	"Invalid regular expression",
	"Invalid collation character",
	"Invalid character class name",
	"Trailing backslash",
	"Invalid back reference",
	"Unmatched [, [^, [:, [., or [=",
	"Unmatched ( or \\(",
	"Unmatched \\{",
	"Invalid content of \\{\\}",
	"Invalid range end",
	"Memory exhausted",
	"Invalid preceding regular expression",
};

size_t regerror(int e, const regex_t *__RESTRICT re, char *__RESTRICT buf, size_t size)
{
	const char *m = e >= 0 && e < (int)(sizeof msgs / sizeof *msgs) ? msgs[e] : "Unknown error";
	size_t l = strlen(m) + 1;
	(void)re;
	if (size) {
		size_t n = l < size ? l : size;
		memcpy(buf, m, n - 1);
		buf[n - 1] = 0;
	}
	return l;
}
