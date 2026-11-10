#include <libgen.h>
#include <string.h>

char *basename(char *s)
{
	size_t i;
	if (!s || !*s) return ".";
	i = strlen(s) - 1;
	while (i && s[i] == '/') s[i--] = 0;
	if (!i) return s;            /* "/" or one character */
	while (i && s[i - 1] != '/') i--;
	return s + i;
}

char *dirname(char *s)
{
	size_t i;
	if (!s || !*s) return ".";
	i = strlen(s) - 1;
	while (s[i] == '/') { if (!i--) return "/"; }
	while (s[i] != '/') { if (!i--) return "."; }
	while (s[i] == '/') { if (!i--) return "/"; }
	s[i + 1] = 0;
	return s;
}
