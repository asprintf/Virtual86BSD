#include <time.h>
#include "time_impl.h"

size_t strftime_l(char *__RESTRICT s, size_t max, const char *__RESTRICT f,
                  const struct tm *__RESTRICT tm, locale_t loc)
{
	return __strftime_l(s, max, f, tm, loc);
}
