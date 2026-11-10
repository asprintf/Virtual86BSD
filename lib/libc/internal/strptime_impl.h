/* Internal: the strptime parser, also used by getdate(). */
#ifndef _INTERNAL_STRPTIME_IMPL_H
#define _INTERNAL_STRPTIME_IMPL_H
#include <time.h>

#define HAVE_YEAR     0x001
#define HAVE_MON      0x002
#define HAVE_MDAY     0x004
#define HAVE_WDAY     0x008
#define HAVE_HOUR     0x010
#define HAVE_MIN      0x020
#define HAVE_SEC      0x040
#define HAVE_YDAY     0x080
#define HAVE_ZONE     0x100
#define HAVE_FULLYEAR 0x200

/* returns the end of the parsed input, or 0; *have tells which fields
 * were set; nocase makes ordinary characters match case-insensitively */
const char *__strptime(const char *s, const char *f, struct tm *tm, int *have, int nocase);
#endif
