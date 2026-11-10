#include <stdlib.h>
#include <string.h>

int getsubopt(char **__RESTRICT opt, char *const *__RESTRICT keys, char **__RESTRICT val)
{
	char *s = *opt, *e;
	int i;
	*val = 0;
	*opt = strchr(s, ',');
	if (*opt) *(*opt)++ = 0;
	else *opt = s + strlen(s);
	e = strchr(s, '=');
	for (i = 0; keys[i]; i++) {
		size_t l = strlen(keys[i]);
		if (strncmp(keys[i], s, l)) continue;
		if (s[l] == '=') { *val = s + l + 1; return i; }
		if (!s[l]) return i;
	}
	if (e) *val = s;   /* unknown: the whole option, as other systems do */
	else *val = s;
	return -1;
}
