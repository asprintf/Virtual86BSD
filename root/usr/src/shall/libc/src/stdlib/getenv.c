#include <stdlib.h>
#include <string.h>
#include "libc.h"

char *getenv(const char *name)
{
	size_t l = strlen(name);
	char **e;
	if (!__environ || !l || strchr(name, '=')) return 0;
	for (e = __environ; *e; e++)
		if (!strncmp(name, *e, l) && (*e)[l] == '=') return *e + l + 1;
	return 0;
}
