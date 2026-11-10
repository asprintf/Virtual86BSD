#include <string.h>
#include <stdlib.h>
/* strdup for library-internal use from objects that must not reference
 * the POSIX name. */
char *__strdup_internal(const char *s)
{
	size_t l = strlen(s) + 1;
	char *d = malloc(l);
	return d ? memcpy(d, s, l) : 0;
}
